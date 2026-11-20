#pragma once
#include <QList>
#include <QString>

// Tasten werden überall als Windows-Tastencodes (Virtual Keys) gespeichert. Die Werte sind auf allen Systemen
// gleich, damit Profile und Makros zwischen Rechnern austauschbar bleiben; nur Windows führt sie auch aus.
namespace vk {
enum : int {
    Backspace = 0x08, Tab = 0x09, Enter = 0x0D, Shift = 0x10, Ctrl = 0x11, Alt = 0x12, Pause = 0x13, CapsLock = 0x14,
    Esc = 0x1B, Space = 0x20, PageUp = 0x21, PageDown = 0x22, End = 0x23, Home = 0x24, Left = 0x25, Up = 0x26,
    Right = 0x27, Down = 0x28, Print = 0x2C, Insert = 0x2D, Delete = 0x2E, Key0 = 0x30, KeyA = 0x41, LWin = 0x5B,
    RWin = 0x5C, Menu = 0x5D, Num0 = 0x60, NumMul = 0x6A, NumAdd = 0x6B, NumSub = 0x6D, NumDec = 0x6E, NumDiv = 0x6F,
    F1 = 0x70, F5 = 0x74, F8 = 0x77, F9 = 0x78, F24 = 0x87, NumLock = 0x90, ScrollLock = 0x91, LShift = 0xA0, RShift = 0xA1, LCtrl = 0xA2, RCtrl = 0xA3,
    LAlt = 0xA4, RAlt = 0xA5, VolMute = 0xAD, VolDown = 0xAE, VolUp = 0xAF, MediaNext = 0xB0, MediaPrev = 0xB1,
    MediaStop = 0xB2, MediaPlay = 0xB3, Oem1 = 0xBA, OemPlus = 0xBB, OemComma = 0xBC, OemMinus = 0xBD, OemPeriod = 0xBE,
    Oem2 = 0xBF, Oem3 = 0xC0, Oem4 = 0xDB, Oem5 = 0xDC, Oem6 = 0xDD, Oem7 = 0xDE, Oem102 = 0xE2,
};
}

QString keyName(int vk);                 // fester Name, z. B. "A", "F5", "Shift", "Space" (für Dateien und Makrotext)
QString keyLabel(int vk);                // Anzeige in der eingestellten Sprache ("Strg", "Leertaste" …)
int keyFromName(const QString& name);    // auch deutsche Namen; 0 = unbekannt
QList<int> allKeys();                    // für die Auswahlliste
bool isModifierKey(int vk);              // Shift/Strg/Alt/Win (auch links/rechts)
int genericKey(int vk);                  // LShift -> Shift usw., sonst vk
bool isExtendedKey(int vk);              // Windows: braucht KEYEVENTF_EXTENDEDKEY beim Senden
int keyFromQt(int qtKey, bool keypad);   // Qt::Key -> Tastencode (0 = keine passende Taste)

// Kombination wie "Strg+Shift+C": Modifikatoren (allgemein: Ctrl/Shift/Alt/LWin) + eine Taste
struct KeyCombo {
    QList<int> mods;
    int key = 0;  // 0 = Taste abschalten
    bool operator==(const KeyCombo& o) const { return mods == o.mods && key == o.key; }
};
QString comboName(const KeyCombo& c);   // "Ctrl+Shift+C"
QString comboLabel(const KeyCombo& c);  // "Strg + Shift + C"
KeyCombo comboFromName(const QString& s);
