#pragma once
#include <QAction>
#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QSystemTrayIcon>
#include <QTimer>
#include "config.h"
#include "engine.h"
#include "overlay.h"
#include "platform.h"
#include "sensors.h"

class MainWindow;
class Updater;

QString serverName();

// Hält alles zusammen: Einstellungen, Messwerte, Overlay, globale Taste, Tastatur-Hook, Profilwechsel und Tray
class App : public QObject {
    Q_OBJECT
public:
    // quiet = Selbsttest/Website-Bilder: kein Tray, kein Hook, keine echte Messung, kein Updater, nichts gespeichert
    explicit App(const Config& c, bool quiet = false, QObject* parent = nullptr);
    ~App() override;

    Config& config() { return cfg_; }
    const Metrics& metrics() const { return metrics_; }
    void changed();               // Einstellungen geändert: anwenden und (verzögert) speichern
    void applyLook();
    void setOverlayVisible(bool on);
    void toggleOverlay();
    int activeProfile() const { return active_; }
    QString activeProgram() const { return program_; }
    QStringList recentPrograms() const { return recent_; }
    bool hotkeyOk() const { return hotkeyOk_; }
    void setMetricsForTest(const Metrics& m);  // Selbsttest und Website-Bilder
    void showWindow();
    void quit();
    void listen();
    void checkUpdates();
    MainWindow* window() const { return win_; }
    OverlayWindow* overlay() { return &overlay_; }
    // Makro-Aufnahme (Windows): Ereignisse landen in recorded(), bis stopRecording()
    void startRecording();
    QList<RecordedKey> stopRecording();
    bool recording() const { return recording_; }

signals:
    void metricsChanged(const Metrics& m);
    void profileChanged(int index, const QString& program);
    void settingsChanged();

private:
    void onMetrics(const Metrics& m);
    void updateProfile(const QString& program);
    void applyEngine();
    void refreshTray();

    Config cfg_;
    bool quiet_;
    Metrics metrics_;
    Sampler sampler_;
    OverlayWindow overlay_;
    GlobalHotkey hotkey_;
    KeyEngine engine_;
    QSystemTrayIcon tray_;
    QMenu menu_;
    QAction* actOverlay_ = nullptr;
    QAction* actRemap_ = nullptr;
    QMenu* profileMenu_ = nullptr;
    QPointer<MainWindow> win_;
    Updater* updater_ = nullptr;
    QTimer saveTimer_;
    int active_ = 0;
    QString program_;
    QStringList recent_;
    QString appliedHotkey_;
    bool hotkeyOk_ = false;
    bool recording_ = false;
    QString pingHost_;
    int interval_ = 0;
    std::shared_ptr<QList<RecordedKey>> rec_;
};
