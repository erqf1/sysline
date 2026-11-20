#pragma once
#include <QHash>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QPair>
#include <QString>
#include <deque>
#include <memory>
#include <mutex>

// Bildrate: woher sie kommt und ob es sie gibt
enum class FpsState {
    Unsupported,  // System kann es nicht (Linux/macOS)
    NeedsAdmin,   // Windows: ETW-Sitzung braucht Adminrechte oder die Gruppe "Leistungsprotokollbenutzer"
    Starting,
    NoFrames,     // läuft, aber das Programm vorne zeichnet gerade nichts über DirectX
    Ok,
};

// -1 = unbekannt/nicht verfügbar
struct Metrics {
    double cpu = -1, gpu = -1;                 // Last in %
    double ramUsed = -1, ramTotal = -1;        // GiB
    double vramUsed = -1, vramTotal = -1;      // GiB
    double gpuTemp = -1, gpuHotspot = -1, cpuTemp = -1;  // °C
    double ping = -1;                          // ms
    bool pingTcp = false;                      // ICMP ging nicht, gemessen wurde der Verbindungsaufbau (Port 443)
    bool pingFailed = false;
    int battery = -1;                          // %
    bool charging = false;
    double fps = -1;
    FpsState fpsState = FpsState::Unsupported;
    QString gpuName, cpuName;
    QString foreground;                        // Pfad des Programms im Vordergrund
    qint64 foregroundPid = 0;
};
Q_DECLARE_METATYPE(Metrics)

// Zählt Bilder (Present-Aufrufe) pro Prozess. Zeit in Sekunden. Quellen: 0 = DXGI, 1 = D3D9, 2 = Kernel (Vulkan/OpenGL);
// gezählt wird die beste Quelle mit Daten, damit dasselbe Bild nicht doppelt zählt.
class FpsCounter {
public:
    void add(quint32 pid, int source, double t);
    double fps(quint32 pid, double now) const;  // -1 = keine Bilder in den letzten 2 s
    void prune(double now);

private:
    struct PerPid {
        std::deque<double> t[3];
    };
    QHash<quint32, PerPid> pids_;
};

// Reine Hilfen (getestet im Selbsttest)
// PDH "GPU Engine"-Instanzen: Summe aller Prozesse je Engine, davon das Maximum (wie der Task-Manager)
double gpuLoadFromEngines(const QList<QPair<QString, double>>& instances, const QString& luid = QString());
// "Zeit=14ms", "time=13.2 ms", "time<1ms" aus der Ausgabe von ping
double parsePingOutput(const QString& out);
// Linux /proc/stat: erste Zeile -> (gesamt, untätig)
bool parseProcStat(const QString& firstLine, quint64* total, quint64* idle);
// Linux /proc/meminfo -> GiB
bool parseMemInfo(const QString& text, double* usedGiB, double* totalGiB);

// Misst in einem eigenen Thread (die Oberfläche hängt nie an einer langsamen Abfrage)
class Sampler : public QObject {
    Q_OBJECT
public:
    explicit Sampler(QObject* parent = nullptr);
    ~Sampler() override;
    void start(int intervalMs, const QString& pingHost);
    void setInterval(int ms);
    void setPingHost(const QString& host);
    void stop();
    static QString cpuTempNote();  // warum es (k)eine CPU-Temperatur gibt

signals:
    void updated(const Metrics& m);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

// Plattformteile (sensors_win/linux/mac.cpp)
struct SystemReader {
    virtual ~SystemReader() = default;
    virtual void read(Metrics& m) = 0;  // im Mess-Thread
    virtual double ping(const QString& host, bool* tcp) = 0;  // ms, -1 = nicht erreichbar
};
std::unique_ptr<SystemReader> makeSystemReader();
QString platformCpuTempNote();

// Bildrate des Vordergrundprogramms (nur Windows über ETW, wie PresentMon)
class FpsSource {
public:
    static FpsSource& instance();
    void start();
    void stop();
    FpsState state() const;
    double fpsFor(quint32 pid) const;
    void inject(quint32 pid, int source, double t);  // Test
};

// Programm im Vordergrund: Pfad der ausführbaren Datei (leer = unbekannt, z. B. Wayland)
QString foregroundProgram(qint64* pid = nullptr);
