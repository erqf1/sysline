// Linux: Autostart (~/.config/autostart), globale Taste über X11 (XGrabKey in eigenem Thread).
// Tasten umbelegen und Makros gibt es hier nicht (bräuchte uinput mit Root-Rechten) - die Oberfläche sagt das.
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QKeySequence>
#include <QStandardPaths>
#include <QWidget>
#include <atomic>
#include <thread>
#include "platform.h"

bool setAutostart(bool on) {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
    const QString path = dir + "/" + kLinuxAppId + ".desktop";
    if (!on) return QFile::remove(path) || !QFile::exists(path);
    QDir().mkpath(dir);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QString("[Desktop Entry]\nType=Application\nName=Sysline\nExec=\"%1\" --background\nIcon=sysline\n"
                    "X-GNOME-Autostart-enabled=true\nNoDisplay=false\n")
                .arg(QCoreApplication::applicationFilePath()).toUtf8());
    return true;
}

bool isElevated() { return false; }
bool restartElevated() { return false; }
bool allowFpsWithoutAdmin() { return false; }
void waitForProcessExit(qint64 pid, int ms) {
    for (int t = 0; t < ms && QDir(QString("/proc/%1").arg(pid)).exists(); t += 50)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
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

// ---------------------------------------------------------------- Globale Taste (X11)
#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <sys/select.h>

namespace {
unsigned long keysymFor(int key) {
    if (key >= Qt::Key_A && key <= Qt::Key_Z) return XK_a + (key - Qt::Key_A);
    if (key >= Qt::Key_0 && key <= Qt::Key_9) return XK_0 + (key - Qt::Key_0);
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) return XK_F1 + (key - Qt::Key_F1);
    switch (key) {
    case Qt::Key_Return: case Qt::Key_Enter: return XK_Return;
    case Qt::Key_Tab: return XK_Tab;
    case Qt::Key_Escape: return XK_Escape;
    case Qt::Key_Space: return XK_space;
    case Qt::Key_Insert: return XK_Insert;
    case Qt::Key_Delete: return XK_Delete;
    case Qt::Key_Home: return XK_Home;
    case Qt::Key_End: return XK_End;
    case Qt::Key_PageUp: return XK_Prior;
    case Qt::Key_PageDown: return XK_Next;
    case Qt::Key_Pause: return XK_Pause;
    case Qt::Key_ScrollLock: return XK_Scroll_Lock;
    case Qt::Key_Print: return XK_Print;
    default: return 0;
    }
}
std::atomic<bool> g_grabFailed{false};
int onXError(Display*, XErrorEvent* e) {
    if (e->error_code == BadAccess) g_grabFailed = true;
    return 0;
}
}  // namespace

struct GlobalHotkey::Impl {
    std::thread thread;
    std::atomic<bool> stop{false};
};

GlobalHotkey::GlobalHotkey(QObject* parent) : QObject(parent), d(new Impl) {}
GlobalHotkey::~GlobalHotkey() { clear(); }

bool GlobalHotkey::supported() {
    if (qEnvironmentVariableIsEmpty("DISPLAY") || !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) return false;
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return false;
    XCloseDisplay(dpy);
    return true;
}

bool GlobalHotkey::set(const QString& sequence) {
    clear();
    const QKeySequence seq(sequence);
    if (seq.isEmpty() || qEnvironmentVariableIsEmpty("DISPLAY")) return false;
    const QKeyCombination kc = seq[0];
    const unsigned long sym = keysymFor(kc.key());
    if (!sym) return false;
    unsigned int mods = 0;
    if (kc.keyboardModifiers() & Qt::ControlModifier) mods |= ControlMask;
    if (kc.keyboardModifiers() & Qt::AltModifier) mods |= Mod1Mask;
    if (kc.keyboardModifiers() & Qt::ShiftModifier) mods |= ShiftMask;
    if (kc.keyboardModifiers() & Qt::MetaModifier) mods |= Mod4Mask;
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return false;
    const Window root = DefaultRootWindow(dpy);
    const KeyCode code = XKeysymToKeycode(dpy, sym);
    g_grabFailed = false;
    XErrorHandler old = XSetErrorHandler(onXError);
    for (unsigned int extra : {0u, unsigned(LockMask), unsigned(Mod2Mask), unsigned(LockMask | Mod2Mask)})
        XGrabKey(dpy, code, mods | extra, root, True, GrabModeAsync, GrabModeAsync);
    XSync(dpy, False);
    XSetErrorHandler(old);
    if (g_grabFailed) {
        appLog("hotkey: X11 grab refused: " + sequence);
        XCloseDisplay(dpy);
        return false;
    }
    d->stop = false;
    Impl* s = d.get();
    d->thread = std::thread([this, s, dpy] {
        const int fd = ConnectionNumber(dpy);
        while (!s->stop) {
            while (XPending(dpy)) {
                XEvent ev;
                XNextEvent(dpy, &ev);
                if (ev.type == KeyPress) QMetaObject::invokeMethod(this, [this] { emit activated(); }, Qt::QueuedConnection);
            }
            fd_set set;
            FD_ZERO(&set);
            FD_SET(fd, &set);
            timeval tv{0, 50000};
            select(fd + 1, &set, nullptr, nullptr, &tv);
        }
        XCloseDisplay(dpy);
    });
    return true;
}

void GlobalHotkey::clear() {
    d->stop = true;
    if (d->thread.joinable()) d->thread.join();
}
#else
struct GlobalHotkey::Impl {};
GlobalHotkey::GlobalHotkey(QObject* parent) : QObject(parent), d(new Impl) {}
GlobalHotkey::~GlobalHotkey() = default;
bool GlobalHotkey::supported() { return false; }
bool GlobalHotkey::set(const QString&) { return false; }
void GlobalHotkey::clear() {}
#endif
