#pragma once
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <mutex>
#include "keys.h"

// ---------------------------------------------------------------- Umbelegen
struct Remap {
    int from = 0;      // gedrückte Taste
    KeyCombo to;       // was stattdessen ankommt (to.key == 0: Taste abgeschaltet)
    bool enabled = true;
};

// ---------------------------------------------------------------- Makros
struct MacroStep {
    enum Kind { Tap, Down, Up, Wait };
    Kind kind = Tap;
    int key = 0;  // Tap/Down/Up
    int ms = 0;   // Wait
    bool operator==(const MacroStep& o) const { return kind == o.kind && key == o.key && ms == o.ms; }
};

struct Macro {
    enum Mode { Once, Repeat, WhileHeld };
    QString name;
    int trigger = 0;     // Taste, die das Makro startet (wird nicht ans Spiel weitergegeben)
    Mode mode = Once;
    int repeat = 3;      // nur bei Repeat
    bool enabled = true;
    QList<MacroStep> steps;
};

// Zeitplan eines Durchlaufs: wann welche Taste gedrückt oder losgelassen wird (ms ab Start)
struct TimedKey {
    int at = 0;
    int key = 0;
    bool down = true;
    bool operator==(const TimedKey& o) const { return at == o.at && key == o.key && down == o.down; }
};
inline constexpr int kTapHoldMs = 20;  // so lange bleibt eine "getippte" Taste unten (Spiele fragen oft nur alle 8-16 ms ab)
QList<TimedKey> macroSchedule(const QList<MacroStep>& steps, int repeats, int* totalMs = nullptr);

// Aufnahme -> Schritte: Pausen gerundet, kurzes Drücken+Loslassen wird zu "Tippen", Tastenwiederholung fällt weg
struct RecordedKey {
    qint64 ms = 0;
    int key = 0;
    bool down = true;
};
QList<MacroStep> stepsFromRecording(const QList<RecordedKey>& events);

// Text zum Bearbeiten: eine Zeile pro Schritt ("tap A", "down Shift", "up Shift", "wait 50"), Deutsch geht auch
QString formatSteps(const QList<MacroStep>& steps, bool german);
QList<MacroStep> parseSteps(const QString& text, int* errorLine = nullptr);
QString macroSummary(const Macro& m);  // "3 Schritte · 0,25 s · einmal"

// ---------------------------------------------------------------- Profile
struct Profile {
    QString name;
    QStringList programs;  // "game.exe", auch mit * und ? ("*-Win64-Shipping.exe")
    bool remapOn = true;
    bool macrosOn = true;
    QList<Remap> remaps;
    QList<Macro> macros;
};
// Profil für das Programm im Vordergrund: erstes passendes ab Index 1, sonst 0 (Standard, gilt für alles)
int pickProfile(const QList<Profile>& profiles, const QString& exePath);
bool programMatches(const QString& pattern, const QString& exePath);

// ---------------------------------------------------------------- Entscheidung pro Tastendruck
struct OutKey {
    int key = 0;
    bool down = true;
    bool operator==(const OutKey& o) const { return key == o.key && down == o.down; }
};
struct KeyDecision {
    bool block = false;      // Original nicht weitergeben
    QList<OutKey> out;       // stattdessen senden
    int startMacro = -1;     // Index im aktiven Profil
    int stopMacro = -1;      // "Solange gedrückt": beim Loslassen anhalten
};

// Wird vom Tastatur-Hook (eigener Thread) gefragt; die Oberfläche setzt Regeln über setProfile(). Was beim
// Drücken entschieden wurde, gilt auch beim Loslassen - auch wenn das Profil dazwischen wechselt.
class KeyEngine {
public:
    void setProfile(const Profile& p, bool remapAllowed, bool macrosAllowed);
    KeyDecision onKey(int key, bool down);
    Macro macro(int index) const;
    void releaseAll(QList<OutKey>* out);  // beim Abschalten: alles, was wir noch "halten", loslassen

private:
    struct Held {
        enum Kind { None, Remapped, MacroKey } kind = None;
        KeyCombo to;
        int macro = -1;
        bool whileHeld = false;
    };
    mutable std::mutex mu_;
    QList<Remap> remaps_;
    QList<Macro> macros_;
    QHash<int, Held> held_;
};
