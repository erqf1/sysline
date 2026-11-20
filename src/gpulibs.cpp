#include "gpulibs.h"

#include <QDir>
#include <QFileInfo>
#include <QVector>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------- NVIDIA NVML
namespace {
using nvmlDevice = void*;
struct NvmlUtil { unsigned int gpu, memory; };
struct NvmlMem { unsigned long long total, free, used; };
using FnInit = int (*)();
using FnCount = int (*)(unsigned int*);
using FnHandle = int (*)(unsigned int, nvmlDevice*);
using FnTemp = int (*)(nvmlDevice, int, unsigned int*);
using FnUtil = int (*)(nvmlDevice, NvmlUtil*);
using FnMem = int (*)(nvmlDevice, NvmlMem*);
using FnName = int (*)(nvmlDevice, char*, unsigned int);
using FnShutdown = int (*)();
}  // namespace

Nvml::~Nvml() {
    if (ok_ && fnShutdown_) reinterpret_cast<FnShutdown>(fnShutdown_)();
}

bool Nvml::open() {
    if (tried_) return ok_;
    tried_ = true;
#if defined(Q_OS_WIN)
    QStringList names = {"nvml"};
    const QString pf = qEnvironmentVariable("ProgramW6432", qEnvironmentVariable("ProgramFiles"));
    if (!pf.isEmpty()) names << QDir(pf).filePath("NVIDIA Corporation/NVSMI/nvml.dll");
#elif defined(Q_OS_LINUX)
    const QStringList names = {"libnvidia-ml.so.1", "libnvidia-ml"};
#else
    const QStringList names;
#endif
    for (const QString& n : names) {
        lib_.setFileName(n);
        if (lib_.load()) break;
    }
    if (!lib_.isLoaded()) return false;
    auto init = reinterpret_cast<FnInit>(lib_.resolve("nvmlInit_v2"));
    auto count = reinterpret_cast<FnCount>(lib_.resolve("nvmlDeviceGetCount_v2"));
    auto handle = reinterpret_cast<FnHandle>(lib_.resolve("nvmlDeviceGetHandleByIndex_v2"));
    fnTemp_ = reinterpret_cast<void*>(lib_.resolve("nvmlDeviceGetTemperature"));
    fnUtil_ = reinterpret_cast<void*>(lib_.resolve("nvmlDeviceGetUtilizationRates"));
    fnMem_ = reinterpret_cast<void*>(lib_.resolve("nvmlDeviceGetMemoryInfo"));
    fnName_ = reinterpret_cast<void*>(lib_.resolve("nvmlDeviceGetName"));
    fnShutdown_ = reinterpret_cast<void*>(lib_.resolve("nvmlShutdown"));
    if (!init || !count || !handle || !fnTemp_) return false;
    if (init() != 0) return false;
    unsigned int n = 0;
    if (count(&n) != 0 || n == 0 || handle(0, &dev_) != 0) {
        if (fnShutdown_) reinterpret_cast<FnShutdown>(fnShutdown_)();
        fnShutdown_ = nullptr;
        return false;
    }
    ok_ = true;
    return true;
}

GpuReading Nvml::read() {
    GpuReading r;
    if (!open()) return r;
    unsigned int t = 0;
    if (reinterpret_cast<FnTemp>(fnTemp_)(dev_, 0 /* NVML_TEMPERATURE_GPU */, &t) == 0) r.temp = t;
    NvmlUtil u{};
    if (fnUtil_ && reinterpret_cast<FnUtil>(fnUtil_)(dev_, &u) == 0) r.load = u.gpu;
    NvmlMem m{};
    if (fnMem_ && reinterpret_cast<FnMem>(fnMem_)(dev_, &m) == 0 && m.total > 0) {
        r.vramTotal = m.total / 1073741824.0;
        r.vramUsed = m.used / 1073741824.0;
    }
    char name[96] = {};
    if (fnName_ && reinterpret_cast<FnName>(fnName_)(dev_, name, sizeof(name) - 1) == 0) r.name = QString::fromUtf8(name);
    r.ok = r.temp >= 0 || r.load >= 0;
    return r;
}

// ---------------------------------------------------------------- AMD ADL (nur Windows)
#if defined(Q_OS_WIN)
namespace {
constexpr int kAdlMaxPath = 256;
struct AdapterInfo {  // aus adl_structures.h (Windows-Fassung)
    int iSize;
    int iAdapterIndex;
    char strUDID[kAdlMaxPath];
    int iBusNumber;
    int iDeviceNumber;
    int iFunctionNumber;
    int iVendorID;
    char strAdapterName[kAdlMaxPath];
    char strDisplayName[kAdlMaxPath];
    int iPresent;
    int iExist;
    char strDriverPath[kAdlMaxPath];
    char strDriverPathExt[kAdlMaxPath];
    char strPNPString[kAdlMaxPath];
    int iOSDisplayIndex;
};
struct AdlSensor { int supported; int value; };
struct AdlPmLog { int size; AdlSensor sensors[256]; };
struct AdlTemperature { int iSize; int iTemperature; };
// Sensor-Nummern aus ADL_PMLOG_SENSORS
constexpr int kPmTempEdge = 8, kPmActivityGfx = 19, kPmTempHotspot = 27;

using AdlContext = void*;
using AdlMalloc = void* (__stdcall*)(int);
void* __stdcall adlAlloc(int size) { return std::malloc(size_t(size)); }
using FnCreate = int (*)(AdlMalloc, int, AdlContext*);
using FnDestroy = int (*)(AdlContext);
using FnNum = int (*)(AdlContext, int*);
using FnInfo = int (*)(AdlContext, AdapterInfo*, int);
using FnActive = int (*)(AdlContext, int, int*);
using FnPmLog = int (*)(AdlContext, int, AdlPmLog*);
using FnOdnTemp = int (*)(AdlContext, int, int, int*);
using FnOd5Temp = int (*)(AdlContext, int, int, AdlTemperature*);
}  // namespace

struct Adl::Impl {
    QLibrary lib;
    AdlContext ctx = nullptr;
    int adapter = -1;
    QString name;
    FnDestroy destroy = nullptr;
    FnPmLog pmlog = nullptr;
    FnOdnTemp odn = nullptr;
    FnOd5Temp od5 = nullptr;
};

Adl::Adl() = default;
Adl::~Adl() {
    if (d && d->ctx && d->destroy) d->destroy(d->ctx);
}

bool Adl::open() {
    if (tried_) return ok_;
    tried_ = true;
    d = std::make_unique<Impl>();
    d->lib.setFileName("atiadlxx");
    if (!d->lib.load()) {
        d->lib.setFileName("atiadlxy");
        if (!d->lib.load()) return false;
    }
    auto create = reinterpret_cast<FnCreate>(d->lib.resolve("ADL2_Main_Control_Create"));
    d->destroy = reinterpret_cast<FnDestroy>(d->lib.resolve("ADL2_Main_Control_Destroy"));
    auto num = reinterpret_cast<FnNum>(d->lib.resolve("ADL2_Adapter_NumberOfAdapters_Get"));
    auto info = reinterpret_cast<FnInfo>(d->lib.resolve("ADL2_Adapter_AdapterInfo_Get"));
    auto active = reinterpret_cast<FnActive>(d->lib.resolve("ADL2_Adapter_Active_Get"));
    d->pmlog = reinterpret_cast<FnPmLog>(d->lib.resolve("ADL2_New_QueryPMLogData_Get"));
    d->odn = reinterpret_cast<FnOdnTemp>(d->lib.resolve("ADL2_OverdriveN_Temperature_Get"));
    d->od5 = reinterpret_cast<FnOd5Temp>(d->lib.resolve("ADL2_Overdrive5_Temperature_Get"));
    if (!create || !num || !info) return false;
    if (create(adlAlloc, 1, &d->ctx) != 0 || !d->ctx) return false;
    int n = 0;
    if (num(d->ctx, &n) != 0 || n <= 0 || n > 64) return false;
    QVector<AdapterInfo> list(n);
    std::memset(list.data(), 0, sizeof(AdapterInfo) * size_t(n));
    for (AdapterInfo& a : list) a.iSize = sizeof(AdapterInfo);
    if (info(d->ctx, list.data(), int(sizeof(AdapterInfo) * size_t(n))) != 0) return false;
    // Mehrere Einträge pro Karte (einer je Anschluss): die erste aktive AMD-Karte nehmen
    for (int pass = 0; pass < 2 && d->adapter < 0; ++pass)
        for (const AdapterInfo& a : list) {
            if (a.iVendorID != 1002) continue;
            int on = 0;
            if (pass == 0 && !(active && active(d->ctx, a.iAdapterIndex, &on) == 0 && on)) continue;
            d->adapter = a.iAdapterIndex;
            d->name = QString::fromLocal8Bit(a.strAdapterName).trimmed();
            break;
        }
    ok_ = d->adapter >= 0;
    return ok_;
}

GpuReading Adl::read() {
    GpuReading r;
    if (!open()) return r;
    r.name = d->name;
    if (d->pmlog) {
        auto* log = new AdlPmLog{};
        if (d->pmlog(d->ctx, d->adapter, log) == 0) {
            const AdlSensor& e = log->sensors[kPmTempEdge];
            const AdlSensor& h = log->sensors[kPmTempHotspot];
            const AdlSensor& a = log->sensors[kPmActivityGfx];
            if (e.supported && e.value > 0 && e.value < 150) r.temp = e.value;
            if (h.supported && h.value > 0 && h.value < 150) r.hotspot = h.value;
            if (a.supported && a.value >= 0 && a.value <= 100) r.load = a.value;
        }
        delete log;
    }
    if (r.temp < 0 && d->odn) {
        int t = 0;
        if (d->odn(d->ctx, d->adapter, 1, &t) == 0 && t > 0) r.temp = t >= 1000 ? t / 1000.0 : t;
    }
    if (r.temp < 0 && d->od5) {
        AdlTemperature t{sizeof(AdlTemperature), 0};
        if (d->od5(d->ctx, d->adapter, 0, &t) == 0 && t.iTemperature > 0) r.temp = t.iTemperature / 1000.0;
    }
    r.ok = r.temp >= 0 || r.load >= 0;
    return r;
}
#else
struct Adl::Impl {};
Adl::Adl() = default;
Adl::~Adl() = default;
bool Adl::open() { return false; }
GpuReading Adl::read() { return {}; }
#endif
