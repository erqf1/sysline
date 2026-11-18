// Linux: /proc (CPU, RAM), sysfs (amdgpu-Last/VRAM/Temperatur, hwmon für die CPU, Akku), NVML für NVIDIA,
// Ping über das System-ping, Programm im Vordergrund über X11 (_NET_ACTIVE_WINDOW). Bildrate: nicht ohne Eingriff ins Spiel.
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <algorithm>
#include "gpulibs.h"
#include "i18n.h"
#include "sensors.h"

#ifdef HAVE_X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

double tcpPing(const QString& host);

namespace {
QString readFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    return QString::fromUtf8(f.readAll()).trimmed();
}

double readNumber(const QString& path) {
    bool ok = false;
    const double v = readFile(path).toDouble(&ok);
    return ok ? v : -1;
}

class LinuxReader : public SystemReader {
public:
    void read(Metrics& m) override {
        if (!init_) init();
        quint64 total = 0, idle = 0;
        QFile st("/proc/stat");
        if (st.open(QIODevice::ReadOnly) && parseProcStat(QString::fromUtf8(st.readLine()), &total, &idle)) {
            if (lastTotal_ && total > lastTotal_)
                m.cpu = std::clamp(100.0 * (1.0 - double(idle - lastIdle_) / double(total - lastTotal_)), 0.0, 100.0);
            lastTotal_ = total;
            lastIdle_ = idle;
        }
        parseMemInfo(readFile("/proc/meminfo"), &m.ramUsed, &m.ramTotal);
        m.cpuName = cpuName_;
        if (!cpuTemp_.isEmpty()) {
            const double t = readNumber(cpuTemp_);
            if (t > 0) m.cpuTemp = t / 1000.0;
        }
        if (!amd_.isEmpty()) {
            m.gpuName = gpuName_;
            const double busy = readNumber(amd_ + "/gpu_busy_percent");
            if (busy >= 0) m.gpu = busy;
            const double used = readNumber(amd_ + "/mem_info_vram_used"), tot = readNumber(amd_ + "/mem_info_vram_total");
            if (used >= 0 && tot > 0) {
                m.vramUsed = used / 1073741824.0;
                m.vramTotal = tot / 1073741824.0;
            }
            if (!amdTemp_.isEmpty()) m.gpuTemp = readNumber(amdTemp_ + "/temp1_input") / 1000.0;
            if (!amdTemp_.isEmpty()) {
                const double h = readNumber(amdTemp_ + "/temp2_input");
                if (h > 0) m.gpuHotspot = h / 1000.0;
            }
        } else {
            const GpuReading g = nvml_.read();
            if (g.ok) {
                m.gpuName = g.name;
                m.gpu = g.load;
                m.gpuTemp = g.temp;
                m.vramUsed = g.vramUsed;
                m.vramTotal = g.vramTotal;
            }
        }
        if (!battery_.isEmpty()) {
            const double cap = readNumber(battery_ + "/capacity");
            if (cap >= 0) m.battery = int(cap);
            const QString s = readFile(battery_ + "/status");
            m.charging = s == "Charging" || s == "Full";
        }
    }

    double ping(const QString& host, bool* tcp) override {
        *tcp = false;
        const QString exe = QStandardPaths::findExecutable("ping");
        if (!exe.isEmpty()) {
            QProcess p;
            p.start(exe, {"-n", "-c", "1", "-W", "2", host});
            if (p.waitForFinished(3500)) {
                const double ms = parsePingOutput(QString::fromLocal8Bit(p.readAllStandardOutput()));
                if (ms >= 0) return ms;
            } else {
                p.kill();
                p.waitForFinished(500);
            }
        }
        *tcp = true;
        return tcpPing(host);
    }

private:
    void init() {
        init_ = true;
        for (const QString& line : readFile("/proc/cpuinfo").split('\n'))
            if (line.startsWith("model name")) {
                cpuName_ = line.section(':', 1).simplified();
                break;
            }
        // CPU-Temperatur: k10temp (AMD, Tctl), zenpower, coretemp (Intel, Package), sonst eine Thermalzone der CPU
        QDir hw("/sys/class/hwmon");
        for (const QString& h : hw.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString dir = hw.filePath(h);
            const QString name = readFile(dir + "/name");
            if (name == "k10temp" || name == "zenpower" || name == "coretemp" || name == "cpu_thermal") {
                QString pick = dir + "/temp1_input";
                for (int i = 1; i <= 16; ++i) {
                    const QString label = readFile(QString("%1/temp%2_label").arg(dir).arg(i));
                    if (label == "Tctl" || label == "Tdie" || label.startsWith("Package")) {
                        pick = QString("%1/temp%2_input").arg(dir).arg(i);
                        break;
                    }
                }
                if (QFileInfo::exists(pick)) {
                    cpuTemp_ = pick;
                    break;
                }
            }
        }
        // AMD-Grafikkarte (amdgpu): Werte direkt aus sysfs
        QDir drm("/sys/class/drm");
        for (const QString& c : drm.entryList({"card*"}, QDir::Dirs | QDir::System)) {
            if (c.contains('-')) continue;
            const QString dev = drm.filePath(c) + "/device";
            if (!QFileInfo::exists(dev + "/gpu_busy_percent")) continue;
            if (readFile(dev + "/vendor") != "0x1002") continue;
            amd_ = dev;
            QDir mon(dev + "/hwmon");
            const QStringList hm = mon.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            if (!hm.isEmpty()) amdTemp_ = mon.filePath(hm.first());
            gpuName_ = readFile(dev + "/product_name");
            if (gpuName_.isEmpty()) gpuName_ = "AMD Radeon";
            break;
        }
        QDir ps("/sys/class/power_supply");
        for (const QString& b : ps.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System))
            if (readFile(ps.filePath(b) + "/type") == "Battery" && QFileInfo::exists(ps.filePath(b) + "/capacity")) {
                battery_ = ps.filePath(b);
                break;
            }
    }

    bool init_ = false;
    quint64 lastTotal_ = 0, lastIdle_ = 0;
    QString cpuTemp_, amd_, amdTemp_, battery_, cpuName_, gpuName_;
    Nvml nvml_;
};
}  // namespace

std::unique_ptr<SystemReader> makeSystemReader() { return std::make_unique<LinuxReader>(); }

QString platformCpuTempNote() {
    return L("On Linux the processor temperature comes from the kernel's own sensor drivers (k10temp, coretemp). Nothing extra is installed.");
}

QString foregroundProgram(qint64* pid) {
    if (pid) *pid = 0;
#ifdef HAVE_X11
    if (qEnvironmentVariableIsEmpty("DISPLAY")) return QString();
    static Display* dpy = XOpenDisplay(nullptr);  // im Mess-Thread, bleibt offen
    if (!dpy) return QString();
    const Atom active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", True), wmPid = XInternAtom(dpy, "_NET_WM_PID", True);
    if (active == None || wmPid == None) return QString();
    auto prop = [&](Window w, Atom a, unsigned long* out) {
        Atom type;
        int format;
        unsigned long n = 0, rest = 0;
        unsigned char* data = nullptr;
        bool ok = false;
        if (XGetWindowProperty(dpy, w, a, 0, 1, False, AnyPropertyType, &type, &format, &n, &rest, &data) == Success && data && n) {
            *out = format == 32 ? *reinterpret_cast<unsigned long*>(data) : 0;
            ok = format == 32;
        }
        if (data) XFree(data);
        return ok;
    };
    unsigned long win = 0, p = 0;
    if (!prop(DefaultRootWindow(dpy), active, &win) || !win) return QString();
    if (!prop(Window(win), wmPid, &p) || !p) return QString();
    if (pid) *pid = qint64(p);
    return QFileInfo(QString("/proc/%1/exe").arg(p)).symLinkTarget();
#else
    return QString();
#endif
}

// Ohne Eingriff ins Spiel (wie MangoHud) gibt es unter Linux keine Bildrate von außen
FpsSource& FpsSource::instance() {
    static FpsSource s;
    return s;
}
void FpsSource::start() {}
void FpsSource::stop() {}
FpsState FpsSource::state() const { return FpsState::Unsupported; }
double FpsSource::fpsFor(quint32) const { return -1; }
void FpsSource::inject(quint32, int, double) {}
