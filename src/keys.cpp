#include "keys.h"

#include <QHash>
#include <QStringList>
#include "i18n.h"

namespace {
struct Named {
    int vk;
    const char* name;  // fester Name
    const char* de;    // deutscher Anzeigename (nullptr = wie name)
};
const Named kNamed[] = {
    {vk::Backspace, "Backspace", "Rücktaste"}, {vk::Tab, "Tab", nullptr}, {vk::Enter, "Enter", nullptr},
    {vk::Shift, "Shift", "Umschalt"}, {vk::Ctrl, "Ctrl", "Strg"}, {vk::Alt, "Alt", nullptr},
    {vk::Pause, "Pause", nullptr}, {vk::CapsLock, "CapsLock", "Feststell"}, {vk::Esc, "Esc", nullptr},
    {vk::Space, "Space", "Leertaste"}, {vk::PageUp, "PageUp", "Bild auf"}, {vk::PageDown, "PageDown", "Bild ab"},
    {vk::End, "End", "Ende"}, {vk::Home, "Home", "Pos1"}, {vk::Left, "Left", "Links"}, {vk::Up, "Up", "Hoch"},
    {vk::Right, "Right", "Rechts"}, {vk::Down, "Down", "Runter"}, {vk::Print, "Print", "Druck"},
    {vk::Insert, "Insert", "Einfg"}, {vk::Delete, "Delete", "Entf"}, {vk::LWin, "Win", nullptr},
    {vk::RWin, "RWin", "Win rechts"}, {vk::Menu, "Menu", "Menütaste"}, {vk::NumMul, "NumMul", "Num *"},
    {vk::NumAdd, "NumAdd", "Num +"}, {vk::NumSub, "NumSub", "Num -"}, {vk::NumDec, "NumDec", "Num ,"},
    {vk::NumDiv, "NumDiv", "Num /"}, {vk::NumLock, "NumLock", "Num"}, {vk::ScrollLock, "ScrollLock", "Rollen"},
    {vk::LShift, "LShift", "Umschalt links"}, {vk::RShift, "RShift", "Umschalt rechts"},
    {vk::LCtrl, "LCtrl", "Strg links"}, {vk::RCtrl, "RCtrl", "Strg rechts"}, {vk::LAlt, "LAlt", "Alt links"},
    {vk::RAlt, "RAlt", "Alt Gr"}, {vk::VolMute, "VolumeMute", "Ton aus"}, {vk::VolDown, "VolumeDown", "Leiser"},
    {vk::VolUp, "VolumeUp", "Lauter"}, {vk::MediaNext, "MediaNext", "Nächster Titel"},
    {vk::MediaPrev, "MediaPrev", "Voriger Titel"}, {vk::MediaStop, "MediaStop", "Stopp"},
    {vk::MediaPlay, "MediaPlay", "Wiedergabe"}, {vk::Oem1, "Oem1", "Ü"}, {vk::OemPlus, "Plus", "+"},
    {vk::OemComma, "Comma", ","}, {vk::OemMinus, "Minus", "-"}, {vk::OemPeriod, "Period", "."},
    {vk::Oem2, "Oem2", "#"}, {vk::Oem3, "Oem3", "Ö"}, {vk::Oem4, "Oem4", "ß"}, {vk::Oem5, "Oem5", "^"},
    {vk::Oem6, "Oem6", "´"}, {vk::Oem7, "Oem7", "Ä"}, {vk::Oem102, "Oem102", "<"},
};

const QHash<int, const Named*>& byVk() {
    static const QHash<int, const Named*> h = [] {
        QHash<int, const Named*> m;
        for (const Named& n : kNamed) m.insert(n.vk, &n);
        return m;
    }();
    return h;
}
}  // namespace

QString keyName(int k) {
    if (k <= 0) return QString();
    if (k >= vk::KeyA && k < vk::KeyA + 26) return QString(QChar('A' + (k - vk::KeyA)));
    if (k >= vk::Key0 && k < vk::Key0 + 10) return QString(QChar('0' + (k - vk::Key0)));
    if (k >= vk::Num0 && k < vk::Num0 + 10) return QString("Num%1").arg(k - vk::Num0);
    if (k >= vk::F1 && k <= vk::F24) return QString("F%1").arg(k - vk::F1 + 1);
    if (auto* n = byVk().value(k)) return QString::fromLatin1(n->name);
    return QString("0x%1").arg(k, 2, 16, QChar('0'));
}

QString keyLabel(int k) {
    if (k <= 0) return L("(off)");
    if (language() == "de")
        if (auto* n = byVk().value(k); n && n->de) return QString::fromUtf8(n->de);
    // Englische Anzeige: Sonderzeichen-Tasten nach US-Belegung
    switch (k) {
    case vk::Oem1: return ";";
    case vk::OemPlus: return "=";
    case vk::OemComma: return ",";
    case vk::OemMinus: return "-";
    case vk::OemPeriod: return ".";
    case vk::Oem2: return "/";
    case vk::Oem3: return "`";
    case vk::Oem4: return "[";
    case vk::Oem5: return "\\";
    case vk::Oem6: return "]";
    case vk::Oem7: return "'";
    case vk::Oem102: return "<";
    case vk::RAlt: return "AltGr";
    case vk::LShift: return "Left Shift";
    case vk::RShift: return "Right Shift";
    case vk::LCtrl: return "Left Ctrl";
    case vk::RCtrl: return "Right Ctrl";
    case vk::LAlt: return "Left Alt";
    case vk::NumMul: return "Num *";
    case vk::NumAdd: return "Num +";
    case vk::NumSub: return "Num -";
    case vk::NumDec: return "Num .";
    case vk::NumDiv: return "Num /";
    default: return keyName(k);
    }
}

int keyFromName(const QString& raw) {
    const QString s = raw.trimmed();
    if (s.isEmpty()) return 0;
    if (s.size() == 1) {
        const QChar c = s[0].toUpper();
        if (c >= 'A' && c <= 'Z') return vk::KeyA + (c.unicode() - 'A');
        if (c >= '0' && c <= '9') return vk::Key0 + (c.unicode() - '0');
    }
    const QString u = s.toLower();
    if (u.size() >= 2 && u[0] == 'f') {
        bool ok = false;
        const int n = u.mid(1).toInt(&ok);
        if (ok && n >= 1 && n <= 24) return vk::F1 + n - 1;
    }
    if (u.startsWith("num") && u.size() == 4 && u[3].isDigit()) return vk::Num0 + (u[3].unicode() - '0');
    if (u.startsWith("0x")) {
        bool ok = false;
        const int v = u.mid(2).toInt(&ok, 16);
        if (ok && v > 0 && v < 256) return v;
    }
    for (const Named& n : kNamed)
        if (u == QString::fromLatin1(n.name).toLower() || (n.de && u == QString::fromUtf8(n.de).toLower())) return n.vk;
    // Kurzformen, die man gern tippt
    static const QHash<QString, int> alias = {{"strg", vk::Ctrl}, {"control", vk::Ctrl}, {"umschalt", vk::Shift},
                                              {"escape", vk::Esc}, {"return", vk::Enter}, {"eingabe", vk::Enter},
                                              {"leer", vk::Space}, {"del", vk::Delete}, {"ins", vk::Insert},
                                              {"pgup", vk::PageUp}, {"pgdn", vk::PageDown}, {"lwin", vk::LWin},
                                              {"windows", vk::LWin}, {"altgr", vk::RAlt}};
    return alias.value(u, 0);
}

QList<int> allKeys() {
    QList<int> out;
    for (int i = 0; i < 26; ++i) out << vk::KeyA + i;
    for (int i = 0; i < 10; ++i) out << vk::Key0 + i;
    for (int i = 0; i < 24; ++i) out << vk::F1 + i;
    for (int i = 0; i < 10; ++i) out << vk::Num0 + i;
    for (const Named& n : kNamed) out << n.vk;
    return out;
}

bool isModifierKey(int k) {
    switch (k) {
    case vk::Shift: case vk::Ctrl: case vk::Alt: case vk::LWin: case vk::RWin:
    case vk::LShift: case vk::RShift: case vk::LCtrl: case vk::RCtrl: case vk::LAlt: case vk::RAlt:
        return true;
    default:
        return false;
    }
}

int genericKey(int k) {
    switch (k) {
    case vk::LShift: case vk::RShift: return vk::Shift;
    case vk::LCtrl: case vk::RCtrl: return vk::Ctrl;
    case vk::LAlt: case vk::RAlt: return vk::Alt;
    case vk::RWin: return vk::LWin;
    default: return k;
    }
}

bool isExtendedKey(int k) {
    switch (k) {
    case vk::Insert: case vk::Delete: case vk::Home: case vk::End: case vk::PageUp: case vk::PageDown:
    case vk::Left: case vk::Right: case vk::Up: case vk::Down: case vk::RCtrl: case vk::RAlt: case vk::NumDiv:
    case vk::LWin: case vk::RWin: case vk::Menu: case vk::NumLock: case vk::Print:
    case vk::VolMute: case vk::VolDown: case vk::VolUp: case vk::MediaNext: case vk::MediaPrev: case vk::MediaStop:
    case vk::MediaPlay:
        return true;
    default:
        return false;
    }
}

int keyFromQt(int key, bool keypad) {
    if (keypad) {
        if (key >= Qt::Key_0 && key <= Qt::Key_9) return vk::Num0 + (key - Qt::Key_0);
        switch (key) {
        case Qt::Key_Asterisk: return vk::NumMul;
        case Qt::Key_Plus: return vk::NumAdd;
        case Qt::Key_Minus: return vk::NumSub;
        case Qt::Key_Slash: return vk::NumDiv;
        case Qt::Key_Period: case Qt::Key_Comma: return vk::NumDec;
        case Qt::Key_Enter: return vk::Enter;
        default: break;
        }
    }
    if (key >= Qt::Key_A && key <= Qt::Key_Z) return vk::KeyA + (key - Qt::Key_A);
    if (key >= Qt::Key_0 && key <= Qt::Key_9) return vk::Key0 + (key - Qt::Key_0);
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) return vk::F1 + (key - Qt::Key_F1);
    switch (key) {
    case Qt::Key_Backspace: return vk::Backspace;
    case Qt::Key_Tab: case Qt::Key_Backtab: return vk::Tab;
    case Qt::Key_Return: case Qt::Key_Enter: return vk::Enter;
    case Qt::Key_Shift: return vk::Shift;
    case Qt::Key_Control: return vk::Ctrl;
    case Qt::Key_Alt: return vk::Alt;
    case Qt::Key_AltGr: return vk::RAlt;
    case Qt::Key_Meta: return vk::LWin;
    case Qt::Key_Pause: return vk::Pause;
    case Qt::Key_CapsLock: return vk::CapsLock;
    case Qt::Key_Escape: return vk::Esc;
    case Qt::Key_Space: return vk::Space;
    case Qt::Key_PageUp: return vk::PageUp;
    case Qt::Key_PageDown: return vk::PageDown;
    case Qt::Key_End: return vk::End;
    case Qt::Key_Home: return vk::Home;
    case Qt::Key_Left: return vk::Left;
    case Qt::Key_Up: return vk::Up;
    case Qt::Key_Right: return vk::Right;
    case Qt::Key_Down: return vk::Down;
    case Qt::Key_Print: return vk::Print;
    case Qt::Key_Insert: return vk::Insert;
    case Qt::Key_Delete: return vk::Delete;
    case Qt::Key_Menu: return vk::Menu;
    case Qt::Key_NumLock: return vk::NumLock;
    case Qt::Key_ScrollLock: return vk::ScrollLock;
    case Qt::Key_VolumeMute: return vk::VolMute;
    case Qt::Key_VolumeDown: return vk::VolDown;
    case Qt::Key_VolumeUp: return vk::VolUp;
    case Qt::Key_MediaNext: return vk::MediaNext;
    case Qt::Key_MediaPrevious: return vk::MediaPrev;
    case Qt::Key_MediaStop: return vk::MediaStop;
    case Qt::Key_MediaPlay: case Qt::Key_MediaTogglePlayPause: return vk::MediaPlay;
    case Qt::Key_Plus: case Qt::Key_Equal: return vk::OemPlus;
    case Qt::Key_Comma: return vk::OemComma;
    case Qt::Key_Minus: return vk::OemMinus;
    case Qt::Key_Period: return vk::OemPeriod;
    case Qt::Key_Slash: case Qt::Key_NumberSign: return vk::Oem2;
    case Qt::Key_Semicolon: case Qt::Key_Udiaeresis: return vk::Oem1;
    case Qt::Key_QuoteLeft: case Qt::Key_Odiaeresis: return vk::Oem3;
    case Qt::Key_BracketLeft: case Qt::Key_ssharp: return vk::Oem4;
    case Qt::Key_Backslash: case Qt::Key_AsciiCircum: case Qt::Key_Dead_Circumflex: return vk::Oem5;
    case Qt::Key_BracketRight: case Qt::Key_Dead_Acute: return vk::Oem6;
    case Qt::Key_Apostrophe: case Qt::Key_Adiaeresis: return vk::Oem7;
    case Qt::Key_Less: case Qt::Key_Greater: return vk::Oem102;
    default: return 0;
    }
}

QString comboName(const KeyCombo& c) {
    QStringList parts;
    for (int m : c.mods) parts << keyName(m);
    parts << (c.key ? keyName(c.key) : QString("Off"));
    return parts.join('+');
}

QString comboLabel(const KeyCombo& c) {
    QStringList parts;
    for (int m : c.mods) parts << keyLabel(m);
    parts << keyLabel(c.key);
    return parts.join(" + ");
}

KeyCombo comboFromName(const QString& s) {
    KeyCombo c;
    const QStringList parts = s.split('+', Qt::SkipEmptyParts);
    for (int i = 0; i < parts.size(); ++i) {
        const QString p = parts[i].trimmed();
        const int k = p.compare("off", Qt::CaseInsensitive) == 0 ? 0 : keyFromName(p);
        if (i + 1 < parts.size()) {
            if (k && !c.mods.contains(genericKey(k))) c.mods << genericKey(k);
        } else {
            c.key = k;
        }
    }
    return c;
}
