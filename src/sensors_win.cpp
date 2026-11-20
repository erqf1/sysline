// Windows: CPU/RAM (Kernel32), GPU-Last und VRAM (PDH "GPU Engine"/"GPU Adapter Memory" wie der Task-Manager),
// GPU-Temperatur (NVML/ADL), Akku, Ping (ICMP), Bildrate (ETW wie PresentMon) und Programm im Vordergrund
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <dxgi.h>
#include <evntcons.h>
#include <evntrace.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <pdh.h>
#include <pdhmsg.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>
#include <QVector>
#include <algorithm>
#include <atomic>
#include <iterator>
#include <mutex>
#include <chrono>
#include <thread>
#include <vector>
#include "gpulibs.h"
#include "i18n.h"
#include "sensors.h"

double tcpPing(const QString& host);

namespace {
quint64 ft(const FILETIME& f) { return (quint64(f.dwHighDateTime) << 32) | f.dwLowDateTime; }

struct Adapter {
    QString luid;  // "luid_0x00000000_0x0000C3F2"
    QString name;
    double vramTotal = -1;
    UINT vendor = 0;
};

// Grafikkarte mit dem meisten eigenen Speicher (die "richtige" Karte, nicht die im Prozessor)
Adapter mainAdapter() {
    Adapter best;
    IDXGIFactory1* f = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&f)))) return best;
    IDXGIAdapter1* a = nullptr;
    SIZE_T bestMem = 0;
    for (UINT i = 0; f->EnumAdapters1(i, &a) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 d{};
        if (SUCCEEDED(a->GetDesc1(&d)) && !(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) && d.DedicatedVideoMemory >= bestMem) {
            bestMem = d.DedicatedVideoMemory;
            best.luid = QString("luid_0x%1_0x%2").arg(quint32(d.AdapterLuid.HighPart), 8, 16, QChar('0'))
                            .arg(quint32(d.AdapterLuid.LowPart), 8, 16, QChar('0')).toUpper().replace("LUID_0X", "luid_0x").replace("_0X", "_0x");
            best.name = QString::fromWCharArray(d.Description).trimmed();
            best.vramTotal = d.DedicatedVideoMemory / 1073741824.0;
            best.vendor = d.VendorId;
        }
        a->Release();
    }
    f->Release();
    return best;
}

QList<QPair<QString, double>> pdhArray(PDH_HCOUNTER c) {
    QList<QPair<QString, double>> out;
    DWORD bytes = 0, count = 0;
    if (PdhGetFormattedCounterArrayW(c, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &bytes, &count, nullptr) != PDH_MORE_DATA) return out;
    std::vector<char> buf(bytes);
    auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buf.data());
    if (PdhGetFormattedCounterArrayW(c, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &bytes, &count, items) != ERROR_SUCCESS) return out;
    for (DWORD i = 0; i < count; ++i)
        if (items[i].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA || items[i].FmtValue.CStatus == PDH_CSTATUS_NEW_DATA)
            out << qMakePair(QString::fromWCharArray(items[i].szName), items[i].FmtValue.doubleValue);
    return out;
}

class WinReader : public SystemReader {
public:
    ~WinReader() override {
        if (query_) PdhCloseQuery(query_);
    }

    void read(Metrics& m) override {
        if (!init_) init();
        // CPU
        FILETIME idle, kernel, user;
        if (GetSystemTimes(&idle, &kernel, &user)) {
            const quint64 i = ft(idle), k = ft(kernel), u = ft(user);
            if (lastTotal_) {
                const quint64 dt = (k + u) - lastTotal_, di = i - lastIdle_;
                if (dt > 0) m.cpu = std::clamp(100.0 * (1.0 - double(di) / double(dt)), 0.0, 100.0);
            }
            lastTotal_ = k + u;
            lastIdle_ = i;
        }
        m.cpuName = cpuName_;
        // RAM
        MEMORYSTATUSEX ms{};
        ms.dwLength = sizeof(ms);
        if (GlobalMemoryStatusEx(&ms)) {
            m.ramTotal = ms.ullTotalPhys / 1073741824.0;
            m.ramUsed = (ms.ullTotalPhys - ms.ullAvailPhys) / 1073741824.0;
        }
        // GPU: PDH
        m.gpuName = adapter_.name;
        if (query_ && PdhCollectQueryData(query_) == ERROR_SUCCESS) {
            if (engines_) {
                const auto list = pdhArray(engines_);
                double v = gpuLoadFromEngines(list, adapter_.luid);
                if (v < 0) v = gpuLoadFromEngines(list);
                if (v >= 0) m.gpu = v;
            }
            if (memory_)
                for (const auto& [name, value] : pdhArray(memory_))
                    if (adapter_.luid.isEmpty() || name.startsWith(adapter_.luid, Qt::CaseInsensitive)) {
                        m.vramUsed = std::max(m.vramUsed, value / 1073741824.0);
                    }
            if (m.vramUsed >= 0) m.vramTotal = adapter_.vramTotal;
        }
        // GPU: Treiber (Temperatur, sonst auch Last/VRAM)
        GpuReading g;
        if (adapter_.vendor == 0x10DE || adapter_.vendor == 0) g = nvml_.read();
        if (!g.ok && (adapter_.vendor == 0x1002 || adapter_.vendor == 0)) g = adl_.read();
        if (g.ok) {
            m.gpuTemp = g.temp;
            m.gpuHotspot = g.hotspot;
            if (m.gpu < 0) m.gpu = g.load;
            if (g.vramTotal > 0) {
                m.vramTotal = g.vramTotal;
                m.vramUsed = g.vramUsed;
            }
            if (m.gpuName.isEmpty()) m.gpuName = g.name;
        }
        // Akku
        SYSTEM_POWER_STATUS ps{};
        if (GetSystemPowerStatus(&ps) && !(ps.BatteryFlag & 128) && ps.BatteryFlag != 255 && ps.BatteryLifePercent <= 100) {
            m.battery = ps.BatteryLifePercent;
            m.charging = ps.ACLineStatus == 1;
        }
    }

    double ping(const QString& host, bool* tcp) override {
        *tcp = false;
        addrinfo hints{};
        hints.ai_family = AF_INET;
        addrinfo* res = nullptr;
        if (!wsa_) {
            WSADATA w;
            wsa_ = WSAStartup(MAKEWORD(2, 2), &w) == 0;
        }
        if (getaddrinfo(host.toUtf8().constData(), nullptr, &hints, &res) != 0 || !res) {
            *tcp = true;
            return tcpPing(host);  // z. B. nur IPv6
        }
        const IPAddr addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr.S_un.S_addr;
        freeaddrinfo(res);
        HANDLE h = IcmpCreateFile();
        if (h == INVALID_HANDLE_VALUE) {
            *tcp = true;
            return tcpPing(host);
        }
        char data[32] = "Sysline";
        std::vector<char> reply(sizeof(ICMP_ECHO_REPLY) + sizeof(data) + 8 + 64);
        const DWORD n = IcmpSendEcho(h, addr, data, sizeof(data), nullptr, reply.data(), DWORD(reply.size()), 1000);
        IcmpCloseHandle(h);
        if (n > 0) {
            const auto* r = reinterpret_cast<ICMP_ECHO_REPLY*>(reply.data());
            if (r->Status == IP_SUCCESS) return std::max<double>(r->RoundTripTime, 0.5);
        }
        // ICMP gesperrt (Firewall): Verbindungsaufbau messen
        *tcp = true;
        return tcpPing(host);
    }

private:
    void init() {
        init_ = true;
        adapter_ = mainAdapter();
        if (PdhOpenQueryW(nullptr, 0, &query_) == ERROR_SUCCESS) {
            if (PdhAddEnglishCounterW(query_, L"\\GPU Engine(*)\\Utilization Percentage", 0, &engines_) != ERROR_SUCCESS) engines_ = nullptr;
            if (PdhAddEnglishCounterW(query_, L"\\GPU Adapter Memory(*)\\Dedicated Usage", 0, &memory_) != ERROR_SUCCESS) memory_ = nullptr;
            PdhCollectQueryData(query_);
        } else {
            query_ = nullptr;
        }
        QSettings cpu("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", QSettings::NativeFormat);
        cpuName_ = cpu.value("ProcessorNameString").toString().simplified();
    }

    bool init_ = false, wsa_ = false;
    Adapter adapter_;
    PDH_HQUERY query_ = nullptr;
    PDH_HCOUNTER engines_ = nullptr, memory_ = nullptr;
    quint64 lastTotal_ = 0, lastIdle_ = 0;
    Nvml nvml_;
    Adl adl_;
    QString cpuName_;
};
}  // namespace

std::unique_ptr<SystemReader> makeSystemReader() { return std::make_unique<WinReader>(); }

QString platformCpuTempNote() {
    return L("Windows only reports the processor temperature to programs with their own kernel driver. Sysline doesn't install one, so this value isn't shown on Windows.");
}

// ---------------------------------------------------------------- Programm im Vordergrund
QString foregroundProgram(qint64* pid) {
    if (pid) *pid = 0;
    HWND w = GetForegroundWindow();
    if (!w) return QString();
    DWORD p = 0;
    GetWindowThreadProcessId(w, &p);
    if (!p) return QString();
    if (pid) *pid = p;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, p);
    if (!h) return QString();
    wchar_t buf[MAX_PATH * 2];
    DWORD len = DWORD(std::size(buf));
    QString path;
    if (QueryFullProcessImageNameW(h, 0, buf, &len)) path = QString::fromWCharArray(buf, int(len));
    CloseHandle(h);
    return path;
}

// ---------------------------------------------------------------- Bildrate über ETW
// Wie PresentMon: eine Echtzeit-Sitzung zählt Present-Aufrufe von DXGI (DirectX 10-12), D3D9 und dem Grafik-Kernel
// (Vulkan/OpenGL). Dafür braucht Windows Adminrechte oder die Gruppe "Leistungsprotokollbenutzer" - sonst sagt
// Sysline das ehrlich, statt zu schätzen.
namespace {
const GUID kDxgi = {0xCA11C036, 0x0102, 0x4A2D, {0xA6, 0xAD, 0xF0, 0x3C, 0xFE, 0xD5, 0xD3, 0xC9}};
const GUID kD3d9 = {0x783ACA0A, 0x790E, 0x4D7F, {0x84, 0x51, 0xAA, 0x85, 0x05, 0x11, 0xC6, 0xB9}};
const GUID kDxgKrnl = {0x802EC45A, 0x1E99, 0x4B83, {0x99, 0x20, 0x87, 0xC9, 0x82, 0x77, 0xBA, 0x9D}};
const wchar_t kSession[] = L"Sysline-FPS";

std::mutex g_mu;
FpsCounter g_counter;
std::atomic<int> g_state{int(FpsState::Starting)};
std::thread g_thread;
TRACEHANDLE g_session = 0, g_trace = INVALID_PROCESSTRACE_HANDLE;
bool g_started = false;

double nowSeconds() {
    FILETIME f;
    GetSystemTimePreciseAsFileTime(&f);
    return double(ft(f)) / 1e7;
}

void WINAPI onEvent(PEVENT_RECORD e) {
    const USHORT id = e->EventHeader.EventDescriptor.Id;
    int src = -1;
    if (IsEqualGUID(e->EventHeader.ProviderId, kDxgi) && (id == 42 || id == 55)) src = 0;  // Present_Start, PresentMPO_Start
    else if (IsEqualGUID(e->EventHeader.ProviderId, kD3d9) && id == 1) src = 1;           // Present_Start
    else if (IsEqualGUID(e->EventHeader.ProviderId, kDxgKrnl) && id == 184) src = 2;      // Present_Info
    if (src < 0) return;
    const double t = double(quint64(e->EventHeader.TimeStamp.QuadPart)) / 1e7;  // FILETIME (Standard bei ProcessTrace)
    std::lock_guard<std::mutex> lock(g_mu);
    g_counter.add(e->EventHeader.ProcessId, src, t);
}

std::vector<char> props() {
    std::vector<char> buf(sizeof(EVENT_TRACE_PROPERTIES) + sizeof(kSession) + 16, 0);
    auto* p = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buf.data());
    p->Wnode.BufferSize = ULONG(buf.size());
    p->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    p->Wnode.ClientContext = 1;  // QPC-Zeitstempel
    p->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    p->FlushTimer = 1;
    p->BufferSize = 64;
    p->MinimumBuffers = 8;
    p->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    return buf;
}

bool enable(const GUID& g, const std::vector<USHORT>& ids) {
    // Nur die gezählten Ereignisse liefern lassen (sonst kommen vom Kernel Tausende pro Sekunde)
    std::vector<char> fbuf(sizeof(EVENT_FILTER_EVENT_ID) + sizeof(USHORT) * ids.size(), 0);
    auto* f = reinterpret_cast<EVENT_FILTER_EVENT_ID*>(fbuf.data());
    f->FilterIn = TRUE;
    f->Count = USHORT(ids.size());
    for (size_t i = 0; i < ids.size(); ++i) f->Events[i] = ids[i];
    EVENT_FILTER_DESCRIPTOR desc{};
    desc.Ptr = ULONGLONG(reinterpret_cast<ULONG_PTR>(f));
    desc.Size = ULONG(fbuf.size());
    desc.Type = EVENT_FILTER_TYPE_EVENT_ID;
    ENABLE_TRACE_PARAMETERS par{};
    par.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
    par.EnableFilterDesc = &desc;
    par.FilterDescCount = 1;
    ULONG rc = EnableTraceEx2(g_session, &g, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_VERBOSE, ~0ULL, 0, 0, &par);
    if (rc != ERROR_SUCCESS)  // ältere Windows-Versionen ohne Filter
        rc = EnableTraceEx2(g_session, &g, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_VERBOSE, ~0ULL, 0, 0, nullptr);
    return rc == ERROR_SUCCESS;
}
}  // namespace

FpsSource& FpsSource::instance() {
    static FpsSource s;
    return s;
}

void FpsSource::start() {
    if (g_started) return;
    g_started = true;
    g_state = int(FpsState::Starting);
    std::vector<char> buf = props();
    auto* p = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buf.data());
    ULONG rc = StartTraceW(&g_session, kSession, p);
    if (rc == ERROR_ALREADY_EXISTS) {  // von einem abgestürzten Lauf übrig
        std::vector<char> stop = props();
        ControlTraceW(0, kSession, reinterpret_cast<EVENT_TRACE_PROPERTIES*>(stop.data()), EVENT_TRACE_CONTROL_STOP);
        buf = props();
        p = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buf.data());
        rc = StartTraceW(&g_session, kSession, p);
    }
    if (rc != ERROR_SUCCESS) {
        g_session = 0;
        g_state = int(rc == ERROR_ACCESS_DENIED ? FpsState::NeedsAdmin : FpsState::Unsupported);
        return;
    }
    const bool any = enable(kDxgi, {42, 55}) | enable(kD3d9, {1}) | enable(kDxgKrnl, {184});
    EVENT_TRACE_LOGFILEW lf{};
    lf.LoggerName = const_cast<LPWSTR>(kSession);
    lf.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    lf.EventRecordCallback = onEvent;
    g_trace = OpenTraceW(&lf);
    if (!any || g_trace == INVALID_PROCESSTRACE_HANDLE) {
        g_state = int(FpsState::Unsupported);
        stop();
        g_started = true;  // nicht ständig neu versuchen
        return;
    }
    g_state = int(FpsState::NoFrames);
    g_thread = std::thread([] { ProcessTrace(&g_trace, 1, nullptr, nullptr); });
}

void FpsSource::stop() {
    if (!g_started) return;
    g_started = false;
    if (g_trace != INVALID_PROCESSTRACE_HANDLE) CloseTrace(g_trace);
    g_trace = INVALID_PROCESSTRACE_HANDLE;
    if (g_session) {
        std::vector<char> buf = props();
        ControlTraceW(g_session, nullptr, reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buf.data()), EVENT_TRACE_CONTROL_STOP);
        g_session = 0;
    }
    if (g_thread.joinable()) g_thread.join();
}

FpsState FpsSource::state() const { return FpsState(g_state.load()); }

double FpsSource::fpsFor(quint32 pid) const {
    std::lock_guard<std::mutex> lock(g_mu);
    const double now = nowSeconds();
    g_counter.prune(now);
    return g_counter.fps(pid, now);
}

void FpsSource::inject(quint32 pid, int source, double t) {
    std::lock_guard<std::mutex> lock(g_mu);
    g_counter.add(pid, source, t);
}
