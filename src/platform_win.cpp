// Windows: Autostart (Run-Schlüssel), Adminrechte, Overlay-Fensterstil, globale Taste (RegisterHotKey),
// Tastatur-Hook (WH_KEYBOARD_LL) und Tasten senden (SendInput)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QDir>
#include <QKeySequence>
#include <QSettings>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include "platform.h"

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

static constexpr ULONG_PTR kSyslineMark = 0x5359534C;  // "SYSL": eigene Tasten erkennt der Hook daran

// ---------------------------------------------------------------- Autostart
bool setAutostart(bool on) {
    QSettings run("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (on) run.setValue("Sysline", QString("\"%1\" --background").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));
    else run.remove("Sysline");
    run.sync();
    return run.status() == QSettings::NoError;
}

// ---------------------------------------------------------------- Adminrechte
bool isElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION e{};
    DWORD len = 0;
    const bool ok = GetTokenInformation(token, TokenElevation, &e, sizeof(e), &len) && e.TokenIsElevated;
    CloseHandle(token);
    return ok;
}

bool restartElevated() {
    const std::wstring exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toStdWString();
    const std::wstring args = QString("--after %1").arg(QCoreApplication::applicationPid()).toStdWString();
    const auto r = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"runas", exe.c_str(), args.c_str(), nullptr, SW_SHOWNORMAL));
    return r > 32;
}

bool allowFpsWithoutAdmin() {
    // S-1-5-32-559 = "Leistungsprotokollbenutzer" / "Performance Log Users" (Name hängt von der Sprache ab, die SID nicht)
    const QString user = qEnvironmentVariable("USERDOMAIN") + "\\" + qEnvironmentVariable("USERNAME");
    const QString cmd = QString("-NoProfile -WindowStyle Hidden -Command \"Add-LocalGroupMember -SID 'S-1-5-32-559' -Member '%1'\"")
                            .arg(QString(user).replace("'", "''"));
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = L"powershell.exe";
    const std::wstring params = cmd.toStdWString();
    sei.lpParameters = params.c_str();
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei) || !sei.hProcess) return false;
    WaitForSingleObject(sei.hProcess, 30000);
    DWORD code = 1;
    GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    return code == 0;
}

void waitForProcessExit(qint64 pid, int ms) {
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, DWORD(pid));
    if (!h) return;
    WaitForSingleObject(h, DWORD(ms));
    CloseHandle(h);
}

// ---------------------------------------------------------------- Overlay
void prepareOverlayWindow(QWidget* w, bool hideFromCapture) {
    HWND h = reinterpret_cast<HWND>(w->winId());
    LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
    ex |= WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
    ex &= ~LONG_PTR(WS_EX_APPWINDOW);
    SetWindowLongPtrW(h, GWL_EXSTYLE, ex);
    SetWindowDisplayAffinity(h, hideFromCapture ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
    keepOverlayOnTop(w);
}

void keepOverlayOnTop(QWidget* w) {
    // Spiele im randlosen Fenster schieben sich gern davor: regelmäßig wieder ganz nach vorn
    SetWindowPos(reinterpret_cast<HWND>(w->winId()), HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
}

// ---------------------------------------------------------------- Globale Taste
namespace {
UINT vkForQt(int key, bool keypad) {
    const int v = keyFromQt(key, keypad);
    return UINT(v);
}

class HotkeyFilter : public QAbstractNativeEventFilter {
public:
    std::function<bool(int)> onHotkey;
    bool nativeEventFilter(const QByteArray&, void* message, qintptr*) override {
        MSG* msg = static_cast<MSG*>(message);
        return msg->message == WM_HOTKEY && onHotkey && onHotkey(int(msg->wParam));
    }
};
}  // namespace

struct GlobalHotkey::Impl {
    HotkeyFilter filter;
    int id = 0;
    bool registered = false;
};

GlobalHotkey::GlobalHotkey(QObject* parent) : QObject(parent), d(new Impl) {
    static int counter = 0x5300;
    d->id = ++counter;
    d->filter.onHotkey = [this](int id) {
        if (id != d->id || !d->registered) return false;
        emit activated();
        return true;
    };
    QCoreApplication::instance()->installNativeEventFilter(&d->filter);
}

GlobalHotkey::~GlobalHotkey() {
    clear();
    QCoreApplication::instance()->removeNativeEventFilter(&d->filter);
}

bool GlobalHotkey::supported() { return true; }

bool GlobalHotkey::set(const QString& sequence) {
    clear();
    const QKeySequence seq(sequence);
    if (seq.isEmpty()) return false;
    const QKeyCombination kc = seq[0];
    const UINT vk = vkForQt(kc.key(), kc.keyboardModifiers() & Qt::KeypadModifier);
    if (!vk) return false;
    UINT mods = MOD_NOREPEAT;
    if (kc.keyboardModifiers() & Qt::ControlModifier) mods |= MOD_CONTROL;
    if (kc.keyboardModifiers() & Qt::AltModifier) mods |= MOD_ALT;
    if (kc.keyboardModifiers() & Qt::ShiftModifier) mods |= MOD_SHIFT;
    if (kc.keyboardModifiers() & Qt::MetaModifier) mods |= MOD_WIN;
    d->registered = RegisterHotKey(nullptr, d->id, mods, vk);
    return d->registered;
}

void GlobalHotkey::clear() {
    if (d->registered) UnregisterHotKey(nullptr, d->id);
    d->registered = false;
}

// ---------------------------------------------------------------- Tasten senden
void sendKeys(const QList<OutKey>& keys) {
    if (keys.isEmpty()) return;
    std::vector<INPUT> in;
    in.reserve(size_t(keys.size()));
    for (const OutKey& k : keys) {
        INPUT i{};
        i.type = INPUT_KEYBOARD;
        const UINT scan = MapVirtualKeyW(UINT(k.key), MAPVK_VK_TO_VSC_EX);
        i.ki.wVk = WORD(k.key);
        i.ki.wScan = WORD(scan & 0xFF);
        i.ki.dwFlags = (k.down ? 0 : KEYEVENTF_KEYUP);
        // Scancode mitsenden: Spiele mit DirectInput/Raw Input lesen nur den
        if (i.ki.wScan) i.ki.dwFlags |= KEYEVENTF_SCANCODE;
        if (isExtendedKey(k.key) || (scan & 0xFF00) == 0xE000) i.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        i.ki.dwExtraInfo = kSyslineMark;
        in.push_back(i);
    }
    SendInput(UINT(in.size()), in.data(), sizeof(INPUT));
}

// ---------------------------------------------------------------- Tastatur-Hook
struct KeyHook::Impl {
    std::thread thread;
    DWORD threadId = 0;
    std::atomic<bool> running{false};
    std::atomic<bool> recording{false};
    KeyEngine* engine = nullptr;
};

namespace {
KeyHook* g_hook = nullptr;
KeyHook::Impl* g_impl = nullptr;

LRESULT CALLBACK hookProc(int code, WPARAM wp, LPARAM lp) {
    if (code != HC_ACTION || !g_impl) return CallNextHookEx(nullptr, code, wp, lp);
    const auto* k = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lp);
    // Eigene gesendete Tasten und die anderer Programme nicht noch einmal umlegen
    if (k->flags & LLKHF_INJECTED) return CallNextHookEx(nullptr, code, wp, lp);
    const bool down = wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN;
    const int key = int(k->vkCode);
    if (g_impl->recording) {
        if (g_hook->onRecord) g_hook->onRecord(RecordedKey{qint64(k->time), key, down});
        return CallNextHookEx(nullptr, code, wp, lp);  // beim Aufnehmen nichts verändern
    }
    const KeyDecision dec = g_impl->engine->onKey(key, down);
    if (!dec.out.isEmpty()) sendKeys(dec.out);
    if (dec.startMacro >= 0 && g_hook->onMacroStart) g_hook->onMacroStart(dec.startMacro, key);
    if (dec.stopMacro >= 0 && g_hook->onMacroStop) g_hook->onMacroStop(key);
    if (dec.block) return 1;
    return CallNextHookEx(nullptr, code, wp, lp);
}
}  // namespace

bool KeyHook::supported() { return true; }

KeyHook& KeyHook::instance() {
    static KeyHook h;
    return h;
}

bool KeyHook::start(KeyEngine* engine) {
    if (!d) d = new Impl;
    if (d->running) return true;
    d->engine = engine;
    g_hook = this;
    g_impl = d;
    std::atomic<int> result{0};
    d->thread = std::thread([this, &result] {
        d->threadId = GetCurrentThreadId();
        MSG msg;
        PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);  // Nachrichtenschlange anlegen
        HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, hookProc, GetModuleHandleW(nullptr), 0);
        result = hook ? 1 : -1;
        if (!hook) return;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        UnhookWindowsHookEx(hook);
    });
    while (result == 0) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    if (result < 0) {
        d->thread.join();
        appLog("keys: SetWindowsHookEx failed");
        return false;
    }
    d->running = true;
    appLog("keys: keyboard hook running");
    return true;
}

void KeyHook::stop() {
    if (!d || !d->running) return;
    PostThreadMessageW(d->threadId, WM_QUIT, 0, 0);
    if (d->thread.joinable()) d->thread.join();
    d->running = false;
    QList<OutKey> rel;
    if (d->engine) d->engine->releaseAll(&rel);
    sendKeys(rel);
}

bool KeyHook::running() const { return d && d->running; }

void KeyHook::setRecording(bool on) {
    if (!d) d = new Impl;
    d->recording = on;
}
