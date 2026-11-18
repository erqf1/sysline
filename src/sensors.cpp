#include "sensors.h"

#include <QElapsedTimer>
#include <QHostAddress>
#include <QHostInfo>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <algorithm>
#include <atomic>

// ---------------------------------------------------------------- Bildrate
void FpsCounter::add(quint32 pid, int source, double t) {
    if (source < 0 || source > 2 || pid == 0) return;
    auto& q = pids_[pid].t[source];
    q.push_back(t);
    while (q.size() > 2000) q.pop_front();
}

void FpsCounter::prune(double now) {
    for (auto it = pids_.begin(); it != pids_.end();) {
        bool any = false;
        for (auto& q : it->t) {
            while (!q.empty() && q.front() < now - 4.0) q.pop_front();
            any |= !q.empty();
        }
        it = any ? std::next(it) : pids_.erase(it);
    }
}

double FpsCounter::fps(quint32 pid, double now) const {
    auto it = pids_.constFind(pid);
    if (it == pids_.constEnd()) return -1;
    for (const auto& q : it->t) {
        if (q.empty() || now - q.back() > 2.0) continue;  // Ereignisse kommen gepuffert, bis ~1 s später
        const double last = q.back();
        int n = 0;
        double first = last;
        for (auto r = q.rbegin(); r != q.rend() && *r >= last - 1.0; ++r) {
            first = *r;
            ++n;
        }
        if (n < 2) return n;
        const double span = last - first;
        return span > 0.2 ? (n - 1) / span : n;
    }
    return -1;
}

// ---------------------------------------------------------------- Hilfen
double gpuLoadFromEngines(const QList<QPair<QString, double>>& instances, const QString& luid) {
    // "pid_1234_luid_0x00000000_0x0000C3F2_phys_0_eng_0_engtype_3D" -> Engine = alles ab "luid_"
    QHash<QString, double> perEngine;
    for (const auto& [name, value] : instances) {
        const int at = name.indexOf("luid_");
        if (at < 0) continue;
        const QString engine = name.mid(at);
        if (!luid.isEmpty() && !engine.startsWith(luid, Qt::CaseInsensitive)) continue;
        perEngine[engine] += std::max(0.0, value);
    }
    double best = -1;
    for (double v : perEngine) best = std::max(best, v);
    return best < 0 ? -1 : std::min(100.0, best);
}

double parsePingOutput(const QString& out) {
    static const QRegularExpression re("(?:time|zeit|temps|tiempo|tempo)\\s*([=<])\\s*([0-9]+(?:[.,][0-9]+)?)\\s*ms",
                                       QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(out);
    if (!m.hasMatch()) return -1;
    QString v = m.captured(2);
    v.replace(',', '.');
    const double ms = v.toDouble();
    return m.captured(1) == "<" ? std::min(ms, 0.5) : ms;
}

bool parseProcStat(const QString& firstLine, quint64* total, quint64* idle) {
    const QStringList f = firstLine.simplified().split(' ');
    if (f.size() < 5 || f[0] != "cpu") return false;
    quint64 sum = 0;
    for (int i = 1; i < f.size() && i <= 8; ++i) sum += f[i].toULongLong();  // ohne guest (steckt in user)
    *total = sum;
    *idle = f[4].toULongLong() + (f.size() > 5 ? f[5].toULongLong() : 0);  // idle + iowait
    return sum > 0;
}

bool parseMemInfo(const QString& text, double* usedGiB, double* totalGiB) {
    double total = -1, avail = -1;
    for (const QString& line : text.split('\n')) {
        const QStringList f = line.simplified().split(' ');
        if (f.size() < 2) continue;
        if (f[0] == "MemTotal:") total = f[1].toDouble();
        else if (f[0] == "MemAvailable:") avail = f[1].toDouble();
    }
    if (total <= 0 || avail < 0) return false;
    *totalGiB = total / 1048576.0;
    *usedGiB = (total - avail) / 1048576.0;
    return true;
}

QString Sampler::cpuTempNote() { return platformCpuTempNote(); }

// ---------------------------------------------------------------- Mess-Thread
struct Sampler::Impl {
    QThread thread, pingThread;
    QObject* worker = nullptr;
    QObject* pinger = nullptr;
    QTimer* timer = nullptr;
    QTimer* pingTimer = nullptr;
    std::unique_ptr<SystemReader> reader;
    std::mutex mu;
    QString host;
    double lastPing = -1;
    bool lastTcp = false, lastFailed = false;
    std::atomic<bool> running{false};
};

Sampler::Sampler(QObject* parent) : QObject(parent), d(new Impl) { qRegisterMetaType<Metrics>("Metrics"); }

Sampler::~Sampler() { stop(); }

void Sampler::start(int intervalMs, const QString& pingHost) {
    if (d->running) return;
    d->running = true;
    d->host = pingHost;
    FpsSource::instance().start();
    d->worker = new QObject;
    d->worker->moveToThread(&d->thread);
    d->pinger = new QObject;
    d->pinger->moveToThread(&d->pingThread);
    connect(&d->thread, &QThread::finished, d->worker, &QObject::deleteLater);
    connect(&d->pingThread, &QThread::finished, d->pinger, &QObject::deleteLater);
    d->thread.start();
    d->pingThread.start();
    Impl* s = d.get();
    QMetaObject::invokeMethod(d->worker, [this, s, intervalMs] {
        s->reader = makeSystemReader();
        s->timer = new QTimer(s->worker);
        s->timer->setInterval(intervalMs);
        auto tick = [this, s] {
            Metrics m;
            m.foreground = foregroundProgram(&m.foregroundPid);
            s->reader->read(m);
            m.fpsState = FpsSource::instance().state();
            if (m.fpsState == FpsState::Ok || m.fpsState == FpsState::NoFrames) {
                m.fps = m.foregroundPid > 0 ? FpsSource::instance().fpsFor(quint32(m.foregroundPid)) : -1;
                m.fpsState = m.fps >= 0 ? FpsState::Ok : FpsState::NoFrames;
            }
            {
                std::lock_guard<std::mutex> lock(s->mu);
                m.ping = s->lastPing;
                m.pingTcp = s->lastTcp;
                m.pingFailed = s->lastFailed;
            }
            emit updated(m);
        };
        connect(s->timer, &QTimer::timeout, s->worker, tick);
        s->timer->start();
        tick();
    });
    QMetaObject::invokeMethod(d->pinger, [s] {
        s->pingTimer = new QTimer(s->pinger);
        s->pingTimer->setInterval(2000);
        auto reader = std::shared_ptr<SystemReader>(makeSystemReader());
        auto tick = [s, reader] {
            QString host;
            {
                std::lock_guard<std::mutex> lock(s->mu);
                host = s->host;
            }
            bool tcp = false;
            const double ms = host.isEmpty() ? -1 : reader->ping(host, &tcp);
            std::lock_guard<std::mutex> lock(s->mu);
            s->lastPing = ms;
            s->lastTcp = tcp;
            s->lastFailed = !host.isEmpty() && ms < 0;
        };
        QObject::connect(s->pingTimer, &QTimer::timeout, s->pinger, tick);
        s->pingTimer->start();
        tick();
    });
}

void Sampler::setInterval(int ms) {
    if (!d->running) return;
    Impl* s = d.get();
    QMetaObject::invokeMethod(d->worker, [s, ms] { if (s->timer) s->timer->setInterval(ms); });
}

void Sampler::setPingHost(const QString& host) {
    std::lock_guard<std::mutex> lock(d->mu);
    d->host = host.trimmed();
    d->lastPing = -1;
    d->lastFailed = false;
}

void Sampler::stop() {
    if (!d->running) return;
    d->running = false;
    Impl* s = d.get();
    QMetaObject::invokeMethod(d->worker, [s] { delete s->timer; s->timer = nullptr; s->reader.reset(); }, Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(d->pinger, [s] { delete s->pingTimer; s->pingTimer = nullptr; }, Qt::BlockingQueuedConnection);
    d->thread.quit();
    d->pingThread.quit();
    d->thread.wait(3000);
    d->pingThread.wait(5000);
    FpsSource::instance().stop();
}

// ---------------------------------------------------------------- TCP-"Ping" (Verbindungsaufbau), wenn ICMP nicht geht
double tcpPing(const QString& host) {
    const QHostInfo info = QHostInfo::fromName(host);
    if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) return -1;
    QHostAddress addr = info.addresses().first();
    for (const QHostAddress& a : info.addresses())
        if (a.protocol() == QAbstractSocket::IPv4Protocol) { addr = a; break; }
    QTcpSocket sock;
    QElapsedTimer t;
    t.start();
    sock.connectToHost(addr, 443);
    if (!sock.waitForConnected(1500)) return -1;
    const double ms = t.nsecsElapsed() / 1e6;
    sock.abort();
    return ms;
}
