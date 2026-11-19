#pragma once
#include <QKeyCombination>
#include <QList>
#include <QObject>
#include <QString>
#include <QWidget>
#include <functional>
#include <memory>
#include "engine.h"

// ---------------------------------------------------------------- System
bool setAutostart(bool on);
void appLog(const QString& line);
QString appLogPath();
inline constexpr const char* kLinuxAppId = "win.mert.Sysline";

// Windows: Adminrechte (für die Bildrate über ETW)
bool isElevated();
bool restartElevated();  // startet Sysline mit Adminrechten neu (UAC-Abfrage); true = neue Instanz läuft an
bool allowFpsWithoutAdmin();  // trägt den Benutzer (mit UAC-Abfrage) in "Leistungsprotokollbenutzer" ein
void waitForProcessExit(qint64 pid, int ms);

// Overlay-Fenster: immer vorne, durchklickbar, nicht in der Taskleiste; optional nicht in Aufnahmen
void prepareOverlayWindow(QWidget* w, bool hideFromCapture);
void keepOverlayOnTop(QWidget* w);

// ---------------------------------------------------------------- Globale Taste (Overlay ein/aus)
class GlobalHotkey : public QObject {
    Q_OBJECT
public:
    explicit GlobalHotkey(QObject* parent = nullptr);
    ~GlobalHotkey() override;
    bool set(const QString& sequence);  // false = belegt oder nicht unterstützt
    void clear();
    static bool supported();

signals:
    void activated();

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

// ---------------------------------------------------------------- Tastatur-Hook (nur Windows)
// Fragt für jeden Tastendruck die KeyEngine, blockiert/ersetzt ihn und meldet Makro-Starts. Läuft in einem
// eigenen Thread mit eigener Nachrichtenschleife, damit Windows den Hook nie wegen einer vollen Oberfläche abhängt.
class KeyHook {
public:
    static bool supported();
    static KeyHook& instance();
    bool start(KeyEngine* engine);
    void stop();
    bool running() const;
    // Makro gestartet/gestoppt (läuft im Hook-Thread - schnell zurückkehren)
    std::function<void(int macroIndex, int triggerKey)> onMacroStart;
    std::function<void(int triggerKey)> onMacroStop;
    // Aufnahme: jede echte Taste (nicht von Sysline gesendet) mit Zeit in ms
    std::function<void(const RecordedKey&)> onRecord;
    void setRecording(bool on);

private:
    struct Impl;
    Impl* d = nullptr;
};

// Tasten senden (Windows: SendInput mit Scancodes, damit auch DirectInput-/Raw-Input-Spiele sie sehen)
void sendKeys(const QList<OutKey>& keys);

// Spielt Makros in eigenen Threads ab; ein Makro läuft nie zweimal gleichzeitig
class MacroPlayer {
public:
    static MacroPlayer& instance();
    void play(const Macro& m, int triggerKey);
    void stop(int triggerKey);  // "Solange gedrückt": Taste losgelassen
    void stopAll();
    int running() const;

private:
    struct Impl;
    Impl* d = nullptr;
    MacroPlayer();
};

// Website-Bilder: die Oberfläche so zeigen wie unter Windows (ohne "nur unter Windows"-Hinweise)
extern bool g_demoUi;
inline bool uiKeysSupported() { return g_demoUi || KeyHook::supported(); }
inline bool uiHotkeySupported() { return g_demoUi || GlobalHotkey::supported(); }
