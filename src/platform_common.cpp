// Plattformunabhängige Teile von platform.h: Protokoll und Makro-Abspieler
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QStandardPaths>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include "platform.h"

QString appLogPath() {
    static const QString path = [] {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (dir.isEmpty()) dir = QDir::tempPath();
        QDir().mkpath(dir);
        return QDir(dir).filePath("sysline.log");
    }();
    return path;
}

void appLog(const QString& line) {
    static std::mutex mu;
    std::lock_guard<std::mutex> lock(mu);
    const QString path = appLogPath();
    if (QFileInfo(path).size() > 512 * 1024) {
        QFile::remove(path + ".old");
        QFile::rename(path, path + ".old");
    }
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text))
        f.write((QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz ") + line + '\n').toUtf8());
}

// ---------------------------------------------------------------- Makros abspielen
struct MacroPlayer::Impl {
    struct Run {
        std::atomic<bool> stop{false};
        std::thread thread;
        std::condition_variable cv;
        std::mutex mu;
    };
    std::mutex mu;
    QHash<int, std::shared_ptr<Run>> runs;  // nach Auslöser-Taste
    std::atomic<int> active{0};
};

MacroPlayer::MacroPlayer() : d(new Impl) {}

MacroPlayer& MacroPlayer::instance() {
    static MacroPlayer p;
    return p;
}

void MacroPlayer::play(const Macro& m, int triggerKey) {
    std::lock_guard<std::mutex> lock(d->mu);
    if (auto it = d->runs.find(triggerKey); it != d->runs.end()) {
        if (!(*it)->stop && (*it)->thread.joinable()) return;  // läuft noch: nicht doppelt
        if ((*it)->thread.joinable()) (*it)->thread.join();
        d->runs.erase(it);
    }
    auto run = std::make_shared<Impl::Run>();
    d->runs.insert(triggerKey, run);
    const bool loop = m.mode == Macro::WhileHeld;
    const QList<TimedKey> plan = macroSchedule(m.steps, m.mode == Macro::Repeat ? m.repeat : 1);
    int total = 0;
    macroSchedule(m.steps, 1, &total);
    Impl* s = d;
    run->thread = std::thread([s, run, plan, loop, total] {
        ++s->active;
        using clock = std::chrono::steady_clock;
        do {
            const auto start = clock::now();
            QList<OutKey> pressed;
            for (const TimedKey& k : plan) {
                {
                    std::unique_lock<std::mutex> l(run->mu);
                    run->cv.wait_until(l, start + std::chrono::milliseconds(k.at), [&] { return run->stop.load(); });
                }
                if (run->stop) break;
                sendKeys({OutKey{k.key, k.down}});
                if (k.down) pressed << OutKey{k.key, false};
                else pressed.removeAll(OutKey{k.key, false});
            }
            if (!pressed.isEmpty()) sendKeys(pressed);  // abgebrochen: nichts gedrückt lassen
            // Leeres Makro in der Schleife: kurz warten statt Dauerlast
            if (loop && total <= 0 && !run->stop) std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } while (loop && !run->stop);
        run->stop = true;
        --s->active;
    });
}

void MacroPlayer::stop(int triggerKey) {
    std::lock_guard<std::mutex> lock(d->mu);
    auto it = d->runs.find(triggerKey);
    if (it == d->runs.end()) return;
    (*it)->stop = true;
    (*it)->cv.notify_all();
}

void MacroPlayer::stopAll() {
    QList<std::shared_ptr<Impl::Run>> all;
    {
        std::lock_guard<std::mutex> lock(d->mu);
        all = d->runs.values();
        d->runs.clear();
    }
    for (auto& r : all) {
        r->stop = true;
        r->cv.notify_all();
        if (r->thread.joinable()) r->thread.join();
    }
}

int MacroPlayer::running() const { return d->active.load(); }
