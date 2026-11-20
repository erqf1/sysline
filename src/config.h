#pragma once
#include <QJsonArray>
#include <QList>
#include <QSettings>
#include <QString>
#include "engine.h"

// Werte im Overlay (Reihenfolge = Reihenfolge der Zeilen)
enum class Item { Fps, Cpu, CpuTemp, Gpu, GpuTemp, Ram, Vram, Ping, Clock, Battery, Count };
inline constexpr int kItemCount = int(Item::Count);
QString itemKey(Item i);  // "fps", "cpu" … (für die Einstellungsdatei)

enum class Corner { TopLeft, TopRight, BottomLeft, BottomRight };

struct OverlayOptions {
    bool visible = true;           // Overlay an (globale Taste schaltet um)
    QString hotkey = "Ctrl+Shift+O";
    Corner corner = Corner::TopLeft;
    int scale = 100;               // Prozent (70..200)
    int opacity = 62;              // Hintergrund in Prozent (0 = nur Schrift mit Schatten)
    bool horizontal = false;       // eine Zeile statt Liste
    bool items[kItemCount] = {true, true, false, true, true, true, true, true, false, false};
    QString screen;                // leer = Hauptbildschirm
    bool hideFromCapture = false;  // Windows: nicht in Aufnahmen/Streams sichtbar
    int margin = 14;               // Abstand zum Bildschirmrand (px)
    bool on(Item i) const { return items[int(i)]; }
};

struct Config {
    // Allgemein
    QString language;              // "de" / "en", leer = Systemsprache
    bool autostart = false;
    bool firstRunDone = false;
    // Aussehen (wie in den anderen Line-Programmen)
    bool dark = true;
    QString accent = "#d61f45";    // Karmin
    bool minimal = false;
    bool acryl = false;
    // Overlay und Messwerte
    OverlayOptions overlay;
    QString pingHost = "mert.win";
    int intervalMs = 1000;
    // Tasten und Makros
    bool remapAllowed = true;      // alle Umbelegungen auf einmal pausieren
    bool macrosAllowed = false;    // Makros sind ab Werk aus (Anti-Cheat / Spielregeln)
    bool autoProfile = true;       // Profil nach Programm im Vordergrund wechseln
    int manualProfile = 0;         // ohne Automatik
    QList<Profile> profiles;       // [0] = Standard (gilt für alle Programme)

    static Config load();
    void save() const;
    static Config load(QSettings& s);
    void save(QSettings& s) const;
    void ensureDefaultProfile();
};

QJsonArray profilesToJson(const QList<Profile>& profiles);
QList<Profile> profilesFromJson(const QJsonArray& a);
