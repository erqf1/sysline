#include "app.h"

#include <QApplication>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QRegularExpression>
#include <mutex>
#include "i18n.h"
#include "look.h"
#include "mainwindow.h"
#include "updater.h"

static std::mutex g_recMu;

QString serverName() {
    QString user = qEnvironmentVariable("USERNAME", qEnvironmentVariable("USER", "user"));
    return "sysline-" + user.remove(QRegularExpression("[^A-Za-z0-9_-]"));
}

App::App(const Config& c, bool quiet, QObject* parent) : QObject(parent), cfg_(c), quiet_(quiet) {
    cfg_.ensureDefaultProfile();
    rec_ = std::make_shared<QList<RecordedKey>>();
    saveTimer_.setSingleShot(true);
    saveTimer_.setInterval(400);
    connect(&saveTimer_, &QTimer::timeout, this, [this] { if (!quiet_) cfg_.save(); });
    connect(&hotkey_, &GlobalHotkey::activated, this, &App::toggleOverlay);
    connect(&sampler_, &Sampler::updated, this, &App::onMetrics);

    // Tray
    tray_.setIcon(syslineIcon());
    tray_.setContextMenu(&menu_);
    connect(&tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) showWindow();
    });

    // Tastatur-Hook: Makros starten im Hook-Thread (schnell), alles andere läuft über die KeyEngine
    KeyHook& hook = KeyHook::instance();
    hook.onMacroStart = [this](int index, int key) { MacroPlayer::instance().play(engine_.macro(index), key); };
    hook.onMacroStop = [](int key) { MacroPlayer::instance().stop(key); };
    auto rec = rec_;
    hook.onRecord = [rec](const RecordedKey& k) {
        std::lock_guard<std::mutex> lock(g_recMu);
        if (rec->size() < 5000) rec->append(k);
    };

    if (!quiet_) {
        Updater::Options uo;
        uo.repo = "erqf1/sysline";
        uo.appName = "Sysline";
        uo.version = APP_VERSION;
        uo.innoAppId = "B7E3C1A4-5D29-4F6B-9A80-3C2E7D41F5A9";  // packaging/windows/sysline.iss
        uo.parent = [this]() -> QWidget* { return win_; };
        uo.texts = [] {
            return UpdaterTexts{L("Update available"),
                                L("%1 %2 is available (you have %3). Update now? The app restarts afterwards."),
                                L("Update now"), L("Ignore this update"), L("Later"), L("Downloading update…"),
                                L("The update couldn't be installed automatically. The download page opens instead."),
                                L("You're using the latest version."), L("Cancel")};
        };
        uo.quit = [this] { quit(); };
        updater_ = new Updater(uo, this);
        updater_->startAutoCheck(20000);
    }

    changed();
    if (!quiet_) {
        sampler_.start(cfg_.intervalMs, cfg_.pingHost);
        pingHost_ = cfg_.pingHost;
        interval_ = cfg_.intervalMs;
        if (KeyHook::supported()) KeyHook::instance().start(&engine_);
        tray_.show();
    }
    appLog(QString("start: Sysline %1, admin=%2").arg(APP_VERSION).arg(isElevated()));
}

App::~App() {
    if (!quiet_) {
        if (saveTimer_.isActive()) cfg_.save();
        KeyHook::instance().stop();
        MacroPlayer::instance().stopAll();
        sampler_.stop();
    }
    delete win_;
}

void App::applyLook() {
    setMinimalLook(cfg_.minimal);
    setAcrylLook(cfg_.acryl);
    qApp->setStyleSheet(styleSheet(makePalette(cfg_.accent, cfg_.dark)));
}

void App::changed() {
    cfg_.ensureDefaultProfile();
    setLanguage(cfg_.language);
    overlay_.setOptions(cfg_.overlay, QColor(cfg_.accent));
    if (quiet_) hotkeyOk_ = true;  // Selbsttest/Bilder: nichts eintragen
    if (!quiet_) {
        if (appliedHotkey_ != cfg_.overlay.hotkey) {
            appliedHotkey_ = cfg_.overlay.hotkey;
            hotkeyOk_ = !appliedHotkey_.isEmpty() && hotkey_.set(appliedHotkey_);
            if (!hotkeyOk_) hotkey_.clear();
        }
        overlay_.setVisible(cfg_.overlay.visible);
        if (pingHost_ != cfg_.pingHost) {
            pingHost_ = cfg_.pingHost;
            sampler_.setPingHost(pingHost_);
        }
        if (interval_ != cfg_.intervalMs) {
            interval_ = cfg_.intervalMs;
            sampler_.setInterval(interval_);
        }
        setAutostart(cfg_.autostart);
        saveTimer_.start();
    }
    updateProfile(program_);
    applyEngine();
    // Später: changed() kommt oft aus einem Menüpunkt, der beim Neuaufbau des Menüs sonst gelöscht würde
    QTimer::singleShot(0, this, &App::refreshTray);
    emit settingsChanged();
}

void App::setOverlayVisible(bool on) {
    cfg_.overlay.visible = on;
    changed();
}

void App::toggleOverlay() { setOverlayVisible(!cfg_.overlay.visible); }

void App::onMetrics(const Metrics& m) {
    metrics_ = m;
    // Programm im Vordergrund merken (für "Programm übernehmen") - Sysline selbst nicht
    const QString self = QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
    if (!m.foreground.isEmpty() && QFileInfo(m.foreground).canonicalFilePath() != self) {
        const QString name = QFileInfo(m.foreground).fileName();
        recent_.removeAll(name);
        recent_.prepend(name);
        while (recent_.size() > 10) recent_.removeLast();
        updateProfile(m.foreground);
    }
    overlay_.setMetrics(m);
    emit metricsChanged(m);
}

void App::setMetricsForTest(const Metrics& m) {
    metrics_ = m;
    if (!m.foreground.isEmpty()) {
        recent_.removeAll(QFileInfo(m.foreground).fileName());
        recent_.prepend(QFileInfo(m.foreground).fileName());
        updateProfile(m.foreground);
    }
    overlay_.setMetrics(m);
    emit metricsChanged(m);
}

void App::updateProfile(const QString& program) {
    if (!program.isEmpty()) program_ = program;
    const int idx = cfg_.autoProfile ? pickProfile(cfg_.profiles, program_) : cfg_.manualProfile;
    if (idx == active_) return;
    active_ = std::clamp(idx, 0, int(cfg_.profiles.size()) - 1);
    appLog(QString("profile: %1 (%2)").arg(cfg_.profiles[active_].name, QFileInfo(program_).fileName()));
    applyEngine();
    QTimer::singleShot(0, this, &App::refreshTray);
    emit profileChanged(active_, program_);
}

void App::applyEngine() {
    active_ = std::clamp(active_, 0, int(cfg_.profiles.size()) - 1);
    engine_.setProfile(cfg_.profiles[active_], cfg_.remapAllowed, cfg_.macrosAllowed);
}

void App::refreshTray() {
    menu_.clear();
    auto* open = menu_.addAction(L("Open Sysline"), this, &App::showWindow);
    QFont f = open->font();
    f.setBold(true);
    open->setFont(f);
    actOverlay_ = menu_.addAction(L("Show overlay"), this, [this](bool on) { setOverlayVisible(on); });
    actOverlay_->setCheckable(true);
    actOverlay_->setChecked(cfg_.overlay.visible);
    if (KeyHook::supported()) {
        actRemap_ = menu_.addAction(L("Key remapping on"), this, [this](bool on) {
            cfg_.remapAllowed = on;
            changed();
        });
        actRemap_->setCheckable(true);
        actRemap_->setChecked(cfg_.remapAllowed);
    }
    profileMenu_ = menu_.addMenu(L("Profile"));
    auto* autoAct = profileMenu_->addAction(L("Switch automatically"), this, [this](bool on) {
        cfg_.autoProfile = on;
        changed();
    });
    autoAct->setCheckable(true);
    autoAct->setChecked(cfg_.autoProfile);
    profileMenu_->addSeparator();
    for (int i = 0; i < cfg_.profiles.size(); ++i) {
        auto* a = profileMenu_->addAction(cfg_.profiles[i].name, this, [this, i] {
            cfg_.autoProfile = false;
            cfg_.manualProfile = i;
            changed();
        });
        a->setCheckable(true);
        a->setChecked(i == active_);
    }
    menu_.addSeparator();
    menu_.addAction(L("Check for updates"), this, &App::checkUpdates);
    menu_.addAction(L("Quit"), this, &App::quit);
    tray_.setToolTip(QString("Sysline · %1: %2").arg(L("Profile"), cfg_.profiles[active_].name));
}

void App::checkUpdates() {
    if (updater_) updater_->check(true);
}

void App::showWindow() {
    if (!win_) win_ = new MainWindow(this);
    win_->show();
    win_->raise();
    win_->activateWindow();
}

void App::quit() {
    if (saveTimer_.isActive()) {
        saveTimer_.stop();
        if (!quiet_) cfg_.save();
    }
    QApplication::quit();
}

void App::listen() {
    auto* server = new QLocalServer(this);
    QLocalServer::removeServer(serverName());
    server->listen(serverName());
    connect(server, &QLocalServer::newConnection, this, [this, server] {
        while (QLocalSocket* s = server->nextPendingConnection()) {
            connect(s, &QLocalSocket::readyRead, this, [this, s] {
                const QByteArray cmd = s->readAll().trimmed();
                if (cmd == "show") showWindow();
                else if (cmd == "overlay") toggleOverlay();
                else if (cmd == "quit") quit();
                s->disconnectFromServer();
            });
            connect(s, &QLocalSocket::disconnected, s, &QObject::deleteLater);
        }
    });
}

void App::startRecording() {
    {
        std::lock_guard<std::mutex> lock(g_recMu);
        rec_->clear();
    }
    recording_ = true;
    KeyHook::instance().setRecording(true);
}

QList<RecordedKey> App::stopRecording() {
    KeyHook::instance().setRecording(false);
    recording_ = false;
    std::lock_guard<std::mutex> lock(g_recMu);
    return *rec_;
}
