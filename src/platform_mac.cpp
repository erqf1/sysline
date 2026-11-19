// macOS: Autostart (LaunchAgent), globale Taste (Carbon RegisterEventHotKey).
// Tasten umbelegen und Makros fehlen hier (bräuchte Bedienungshilfen-Rechte und einen Event-Tap) - die Oberfläche sagt das.
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QKeySequence>
#include <QVariant>
#include <QWidget>
#include <thread>
#include "platform.h"

#include <Carbon/Carbon.h>
#include <signal.h>

bool setAutostart(bool on) {
    const QString path = QDir::homePath() + "/Library/LaunchAgents/win.mert.sysline.plist";
    if (!on) return QFile::remove(path) || !QFile::exists(path);
    QDir().mkpath(QDir::homePath() + "/Library/LaunchAgents");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QString(R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key><string>win.mert.sysline</string>
    <key>ProgramArguments</key><array><string>%1</string><string>--background</string></array>
    <key>RunAtLoad</key><true/>
    <key>ProcessType</key><string>Interactive</string>
</dict>
</plist>
)").arg(QCoreApplication::applicationFilePath().toHtmlEscaped()).toUtf8());
    return true;
}

bool isElevated() { return false; }
bool restartElevated() { return false; }
bool allowFpsWithoutAdmin() { return false; }
void waitForProcessExit(qint64 pid, int ms) {
    for (int t = 0; t < ms && kill(pid_t(pid), 0) == 0; t += 50) std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
void prepareOverlayWindow(QWidget*, bool) {}
void keepOverlayOnTop(QWidget* w) { w->raise(); }

void sendKeys(const QList<OutKey>&) {}
struct KeyHook::Impl {};
bool KeyHook::supported() { return false; }
KeyHook& KeyHook::instance() {
    static KeyHook h;
    return h;
}
bool KeyHook::start(KeyEngine*) { return false; }
void KeyHook::stop() {}
bool KeyHook::running() const { return false; }
void KeyHook::setRecording(bool) {}

// ---------------------------------------------------------------- Globale Taste (Carbon)
namespace {
int macKeyCode(int key) {
    static const int letters[26] = {kVK_ANSI_A, kVK_ANSI_B, kVK_ANSI_C, kVK_ANSI_D, kVK_ANSI_E, kVK_ANSI_F, kVK_ANSI_G,
                                    kVK_ANSI_H, kVK_ANSI_I, kVK_ANSI_J, kVK_ANSI_K, kVK_ANSI_L, kVK_ANSI_M, kVK_ANSI_N,
                                    kVK_ANSI_O, kVK_ANSI_P, kVK_ANSI_Q, kVK_ANSI_R, kVK_ANSI_S, kVK_ANSI_T, kVK_ANSI_U,
                                    kVK_ANSI_V, kVK_ANSI_W, kVK_ANSI_X, kVK_ANSI_Y, kVK_ANSI_Z};
    static const int digits[10] = {kVK_ANSI_0, kVK_ANSI_1, kVK_ANSI_2, kVK_ANSI_3, kVK_ANSI_4,
                                   kVK_ANSI_5, kVK_ANSI_6, kVK_ANSI_7, kVK_ANSI_8, kVK_ANSI_9};
    static const int fkeys[12] = {kVK_F1, kVK_F2, kVK_F3, kVK_F4, kVK_F5, kVK_F6, kVK_F7, kVK_F8, kVK_F9, kVK_F10, kVK_F11, kVK_F12};
    if (key >= Qt::Key_A && key <= Qt::Key_Z) return letters[key - Qt::Key_A];
    if (key >= Qt::Key_0 && key <= Qt::Key_9) return digits[key - Qt::Key_0];
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) return fkeys[key - Qt::Key_F1];
    switch (key) {
    case Qt::Key_Space: return kVK_Space;
    case Qt::Key_Return: return kVK_Return;
    case Qt::Key_Tab: return kVK_Tab;
    case Qt::Key_Escape: return kVK_Escape;
    case Qt::Key_Home: return kVK_Home;
    case Qt::Key_End: return kVK_End;
    case Qt::Key_PageUp: return kVK_PageUp;
    case Qt::Key_PageDown: return kVK_PageDown;
    default: return -1;
    }
}

OSStatus hotkeyHandler(EventHandlerCallRef, EventRef event, void* userData);
}  // namespace

struct GlobalHotkey::Impl {
    EventHotKeyRef ref = nullptr;
    EventHandlerRef handler = nullptr;
    UInt32 uid = 0;
};

namespace {
OSStatus hotkeyHandler(EventHandlerCallRef, EventRef event, void* userData) {
    auto* hk = static_cast<GlobalHotkey*>(userData);
    EventHotKeyID id{};
    GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, nullptr, sizeof(id), nullptr, &id);
    if (id.signature != 'SYSL' || id.id != hk->property("_sysline_uid").toUInt()) return eventNotHandledErr;
    QMetaObject::invokeMethod(hk, [hk] { emit hk->activated(); }, Qt::QueuedConnection);
    return noErr;
}
}  // namespace

GlobalHotkey::GlobalHotkey(QObject* parent) : QObject(parent), d(new Impl) {
    static UInt32 counter = 0;
    d->uid = ++counter;
    setProperty("_sysline_uid", uint(d->uid));
    EventTypeSpec spec{kEventClassKeyboard, kEventHotKeyPressed};
    InstallApplicationEventHandler(&hotkeyHandler, 1, &spec, this, &d->handler);
}

GlobalHotkey::~GlobalHotkey() {
    clear();
    if (d->handler) RemoveEventHandler(d->handler);
}

bool GlobalHotkey::supported() { return true; }

bool GlobalHotkey::set(const QString& sequence) {
    clear();
    const QKeySequence seq(sequence);
    if (seq.isEmpty()) return false;
    const QKeyCombination kc = seq[0];
    const int code = macKeyCode(kc.key());
    if (code < 0) return false;
    UInt32 mods = 0;
    const Qt::KeyboardModifiers m = kc.keyboardModifiers();
    if (m & Qt::ControlModifier) mods |= cmdKey;  // Qt: Ctrl = Command auf dem Mac
    if (m & Qt::MetaModifier) mods |= controlKey;
    if (m & Qt::AltModifier) mods |= optionKey;
    if (m & Qt::ShiftModifier) mods |= shiftKey;
    const EventHotKeyID id{'SYSL', d->uid};
    return RegisterEventHotKey(UInt32(code), mods, id, GetApplicationEventTarget(), 0, &d->ref) == noErr;
}

void GlobalHotkey::clear() {
    if (d->ref) UnregisterEventHotKey(d->ref);
    d->ref = nullptr;
}
