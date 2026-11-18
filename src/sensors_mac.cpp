// macOS: CPU-Last (host_statistics), RAM (vm_statistics64), GPU-Last (IOAccelerator "Device Utilization %"),
// Akku (IOPowerSources), Ping über das System-ping, Programm vorne über die Fensterliste. Temperaturen und Bildrate:
// nur über nicht öffentliche Schnittstellen - deshalb nicht.
#include <QFileInfo>
#include <QProcess>
#include <algorithm>
#include "i18n.h"
#include "sensors.h"

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <libproc.h>
#include <mach/mach.h>
#include <sys/sysctl.h>

double tcpPing(const QString& host);

namespace {
class MacReader : public SystemReader {
public:
    void read(Metrics& m) override {
        if (!init_) init();
        host_cpu_load_info_data_t info;
        mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
        if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, reinterpret_cast<host_info_t>(&info), &count) == KERN_SUCCESS) {
            quint64 total = 0;
            for (int i = 0; i < CPU_STATE_MAX; ++i) total += info.cpu_ticks[i];
            const quint64 idle = info.cpu_ticks[CPU_STATE_IDLE];
            if (lastTotal_ && total > lastTotal_)
                m.cpu = std::clamp(100.0 * (1.0 - double(idle - lastIdle_) / double(total - lastTotal_)), 0.0, 100.0);
            lastTotal_ = total;
            lastIdle_ = idle;
        }
        m.cpuName = cpuName_;
        vm_statistics64_data_t vm;
        mach_msg_type_number_t vc = HOST_VM_INFO64_COUNT;
        if (memTotal_ > 0 &&
            host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm), &vc) == KERN_SUCCESS) {
            vm_size_t page = 0;
            host_page_size(mach_host_self(), &page);
            const double used = double(vm.active_count + vm.wire_count + vm.compressor_page_count) * double(page);
            m.ramUsed = used / 1073741824.0;
            m.ramTotal = memTotal_ / 1073741824.0;
        }
        readGpu(m);
        readBattery(m);
    }

    double ping(const QString& host, bool* tcp) override {
        *tcp = false;
        QProcess p;
        p.start("/sbin/ping", {"-n", "-c", "1", "-t", "2", host});
        if (p.waitForFinished(3500)) {
            const double ms = parsePingOutput(QString::fromLocal8Bit(p.readAllStandardOutput()));
            if (ms >= 0) return ms;
        } else {
            p.kill();
            p.waitForFinished(500);
        }
        *tcp = true;
        return tcpPing(host);
    }

private:
    void init() {
        init_ = true;
        uint64_t mem = 0;
        size_t len = sizeof(mem);
        if (sysctlbyname("hw.memsize", &mem, &len, nullptr, 0) == 0) memTotal_ = double(mem);
        char brand[128] = {};
        len = sizeof(brand) - 1;
        if (sysctlbyname("machdep.cpu.brand_string", brand, &len, nullptr, 0) == 0) cpuName_ = QString::fromUtf8(brand).simplified();
    }

    static void readGpu(Metrics& m) {
        io_iterator_t it = 0;
        if (IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching("IOAccelerator"), &it) != KERN_SUCCESS) return;
        io_object_t svc;
        while ((svc = IOIteratorNext(it))) {
            CFMutableDictionaryRef props = nullptr;
            if (IORegistryEntryCreateCFProperties(svc, &props, kCFAllocatorDefault, 0) == KERN_SUCCESS && props) {
                auto perf = static_cast<CFDictionaryRef>(CFDictionaryGetValue(props, CFSTR("PerformanceStatistics")));
                if (perf && CFGetTypeID(perf) == CFDictionaryGetTypeID()) {
                    auto util = static_cast<CFNumberRef>(CFDictionaryGetValue(perf, CFSTR("Device Utilization %")));
                    int v = -1;
                    if (util && CFGetTypeID(util) == CFNumberGetTypeID() && CFNumberGetValue(util, kCFNumberIntType, &v) && v >= 0)
                        m.gpu = std::max(m.gpu, double(std::min(v, 100)));
                }
                CFRelease(props);
            }
            IOObjectRelease(svc);
        }
        IOObjectRelease(it);
    }

    static void readBattery(Metrics& m) {
        CFTypeRef info = IOPSCopyPowerSourcesInfo();
        if (!info) return;
        CFArrayRef list = IOPSCopyPowerSourcesList(info);
        if (list) {
            for (CFIndex i = 0; i < CFArrayGetCount(list); ++i) {
                CFDictionaryRef d = IOPSGetPowerSourceDescription(info, CFArrayGetValueAtIndex(list, i));
                if (!d) continue;
                auto cap = static_cast<CFNumberRef>(CFDictionaryGetValue(d, CFSTR(kIOPSCurrentCapacityKey)));
                auto max = static_cast<CFNumberRef>(CFDictionaryGetValue(d, CFSTR(kIOPSMaxCapacityKey)));
                int c = 0, mx = 0;
                if (cap && max && CFNumberGetValue(cap, kCFNumberIntType, &c) && CFNumberGetValue(max, kCFNumberIntType, &mx) && mx > 0) {
                    m.battery = std::clamp(c * 100 / mx, 0, 100);
                    auto state = static_cast<CFStringRef>(CFDictionaryGetValue(d, CFSTR(kIOPSPowerSourceStateKey)));
                    m.charging = state && CFStringCompare(state, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo;
                    break;
                }
            }
            CFRelease(list);
        }
        CFRelease(info);
    }

    bool init_ = false;
    quint64 lastTotal_ = 0, lastIdle_ = 0;
    double memTotal_ = 0;
    QString cpuName_;
};
}  // namespace

std::unique_ptr<SystemReader> makeSystemReader() { return std::make_unique<MacReader>(); }

QString platformCpuTempNote() {
    return L("macOS only shares temperatures through private interfaces that can change with every update. Sysline leaves them out instead of guessing.");
}

QString foregroundProgram(qint64* pid) {
    if (pid) *pid = 0;
    CFArrayRef wins = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
    if (!wins) return QString();
    int owner = 0;
    for (CFIndex i = 0; i < CFArrayGetCount(wins) && !owner; ++i) {
        auto w = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(wins, i));
        auto layer = static_cast<CFNumberRef>(CFDictionaryGetValue(w, kCGWindowLayer));
        int l = -1;
        if (!layer || !CFNumberGetValue(layer, kCFNumberIntType, &l) || l != 0) continue;
        auto op = static_cast<CFNumberRef>(CFDictionaryGetValue(w, kCGWindowOwnerPID));
        if (op) CFNumberGetValue(op, kCFNumberIntType, &owner);
    }
    CFRelease(wins);
    if (!owner) return QString();
    if (pid) *pid = owner;
    char path[PROC_PIDPATHINFO_MAXSIZE] = {};
    if (proc_pidpath(owner, path, sizeof(path)) <= 0) return QString();
    return QString::fromUtf8(path);
}

FpsSource& FpsSource::instance() {
    static FpsSource s;
    return s;
}
void FpsSource::start() {}
void FpsSource::stop() {}
FpsState FpsSource::state() const { return FpsState::Unsupported; }
double FpsSource::fpsFor(quint32) const { return -1; }
void FpsSource::inject(quint32, int, double) {}
