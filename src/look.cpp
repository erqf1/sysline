#include "look.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QScrollBar>

#include <algorithm>
#include <QAbstractScrollArea>
#include <QScrollBar>
#include <QVariantAnimation>
#include <random>
#include <QWheelEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <cmath>

static bool g_minimal = false;
void setMinimalLook(bool on) { g_minimal = on; }
bool minimalLook() { return g_minimal; }
static bool g_acryl = false;
void setAcrylLook(bool on) { g_acryl = on; }
bool acrylLook() { return g_acryl; }

const QList<QColor>& accentChoices() {
    // Karmin ist die Sysline-Farbe. Bewusst ohne Orange (neben Grün wirkt das wie eine Ampel).
    static const QList<QColor> c = {QColor("#d61f45"), QColor("#ec4899"), QColor("#8b5cf6"), QColor("#3b82f6"),
                                    QColor("#06b6d4"), QColor("#22c55e"), QColor("#64748b")};
    return c;
}

Palette makePalette(const QString& accent, bool dark) {
    Palette p;
    p.accent = QColor(accent).isValid() ? QColor(accent) : QColor("#d61f45");
    if (g_acryl) {  // Acryl: die Farben gelten für Deckendes wie Menüs, Flächen sind durchscheinend (kAcrylSheet)
        if (dark) {
            p.bg = "#1f2023"; p.panel = "#27282c"; p.panel2 = "#303136"; p.border = "#3f4046";
            p.text = "#f1f1f3"; p.muted = "#afb0b8"; p.hover = "#3d3e44";
        } else {
            p.bg = "#eaeaed"; p.panel = "#f6f6f8"; p.panel2 = "#ffffff"; p.border = "#cfcfd6";
            p.text = "#17181c"; p.muted = "#5b5c66"; p.hover = "#dadbe1";
        }
        p.accentText = p.accent.lightnessF() > 0.62 ? QColor("#15161c") : QColor("#ffffff");
        return p;
    }
    if (!g_minimal) {  // Standard: das bisherige Aussehen
        if (dark) {
            p.bg = "#0c0d12"; p.panel = "#14161e"; p.panel2 = "#1b1e29"; p.border = "#2a2e3c";
            p.text = "#eceef4"; p.muted = "#8d93a6";
        } else {
            p.bg = "#f4f5f9"; p.panel = "#ffffff"; p.panel2 = "#eef0f5"; p.border = "#d9dce5";
            p.text = "#151822"; p.muted = "#636a7d";
        }
        p.hover = p.border;
        p.accentText = p.accent.lightnessF() > 0.62 ? QColor("#15161c") : QColor("#ffffff");
        return p;
    }
    // Minimal: neutrale Grautöne, keine Rahmen
    if (dark) {
        p.bg = "#0f0f0f"; p.panel = "#181818"; p.panel2 = "#272727"; p.border = "#303030";
        p.text = "#f1f1f1"; p.muted = "#aaaaaa"; p.hover = "#3f3f3f";
    } else {
        p.bg = "#ffffff"; p.panel = "#f9f9f9"; p.panel2 = "#f2f2f2"; p.border = "#e5e5e5";
        p.text = "#0f0f0f"; p.muted = "#606060"; p.hover = "#e5e5e5";
    }
    p.accentText = p.accent.lightnessF() > 0.62 ? QColor("#15161c") : QColor("#ffffff");
    return p;
}

// Stil Minimal: runde Knöpfe ohne Rahmen, ruhige Grautöne, Auswahl wie Chips (gewählt = Textfarbe als Fläche)
static const char* kMinimalSheet = R"(
* { font-family: 'Roboto','Segoe UI','SF Pro Text','Helvetica Neue','Ubuntu','Noto Sans',sans-serif; font-size: 13px; color: @text@; }
QWidget#root, QDialog, QMainWindow { background: @bg@; }
QLabel { background: transparent; }
QLabel#muted { color: @muted@; }
QLabel#h1 { font-size: 24px; font-weight: 700; }
QLabel#h2 { font-size: 17px; font-weight: 650; }
QLabel#big { font-size: 30px; font-weight: 750; color: @accent@; }
QLabel#pill { background: @panel2@; border: 1px solid @border@; border-radius: 12px; padding: 4px 12px; }
QFrame#card { background: @panel@; border: none; border-radius: 12px; }
QFrame#side { background: @panel@; border: none; }
QPushButton { background: @panel2@; border: none; border-radius: 18px; padding: 8px 16px; font-weight: 500; }
QPushButton:hover { background: @hover@; }
QPushButton:pressed { background: @border@; }
QPushButton#dashed { background: transparent; border: 2px dashed @border@; border-radius: 12px; padding: 11px; color: @muted@; font-weight: 600; }
QPushButton#dashed:hover { border-color: @text@; color: @text@; }
QPushButton:disabled { color: @muted@; }
QPushButton#primary { background: @accent@; color: @acctext@; border-color: @accent@; font-weight: 650; }
QPushButton#primary:hover { background: @accent2@; }
QPushButton#flat { background: transparent; border: none; color: @muted@; padding: 6px 8px; border-radius: 16px; }
QPushButton#flat:hover { background: @panel2@; }
QPushButton#flat:hover { color: @text@; }
QPushButton#hotkey { font-size: 20px; font-weight: 700; letter-spacing: 1px; padding: 12px; border-radius: 12px;
    border: 2px solid @border@; border-bottom-width: 5px; background: @panel2@; }
QPushButton#hotkey:hover { border-color: @muted@; background: @panel2@; }
QPushButton#hotkey[recording="true"] { border-color: @accent@; color: @accent@; background: @panel@; }
QPushButton#swatch { border-radius: 15px; min-width: 30px; max-width: 30px; min-height: 30px; max-height: 30px; padding: 0; border: 2px solid transparent; }
QPushButton#swatch:checked { border: 3px solid @text@; }
/* Auswahl als Chips: gewählt = Textfarbe als Fläche */
QPushButton#seg { border-radius: 8px; padding: 7px 14px; margin-right: 8px; }
QPushButton#seg:checked { background: @text@; color: @bg@; }
QComboBox, QLineEdit, QKeySequenceEdit { background: @bg@; border: 1px solid @border@; border-radius: 8px; padding: 6px 9px; min-height: 20px; }
QComboBox:hover, QLineEdit:hover { border-color: @muted@; }
QComboBox:focus, QLineEdit:focus { border-color: @muted@; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView { background: @panel@; border: 1px solid @border@; outline: 0; selection-background-color: @accent@; selection-color: @acctext@; }
QCheckBox { spacing: 10px; }
@toggle@
QSlider::groove:horizontal { height: 4px; background: @border@; border-radius: 2px; }
QSlider::sub-page:horizontal { background: @accent@; border-radius: 2px; }
QSlider::handle:horizontal { width: 16px; height: 16px; margin: -6px 0; background: @accent@; border-radius: 8px; }
QProgressBar { background: @panel2@; border: none; border-radius: 5px; height: 10px; }
QProgressBar::chunk { background: @accent@; border-radius: 5px; }
QListView { background: transparent; border: none; outline: 0; }
QListWidget { background: transparent; border: none; padding: 4px; }
QListWidget::item { border-radius: 8px; }
QListWidget::item:hover { background: @panel2@; }
QListWidget::item:selected { background: @hover@; color: @text@; }
QScrollBar:vertical { width: 10px; background: transparent; }
QScrollBar::handle:vertical { background: @hover@; border-radius: 5px; min-height: 30px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QTabWidget::pane { border: none; border-top: 1px solid @border@; border-radius: 0; background: @bg@; top: -1px; }
QTabBar::tab { background: transparent; border: none; padding: 10px 18px; color: @muted@; font-weight: 600; }
QTabBar::tab:hover { color: @text@; }
QTabBar::tab:selected { color: @text@; border-bottom: 3px solid @text@; }
QMenu { background: @panel2@; border: none; border-radius: 12px; padding: 8px 0; }
QMenu::item { padding: 8px 36px 8px 16px; }
QMenu::item:selected { background: @hover@; }
QMenu::separator { height: 1px; background: @border@; margin: 6px 0; }
QToolTip { background: @panel2@; color: @text@; border: none; border-radius: 6px; padding: 6px 8px; }
QMessageBox { background: @bg@; }
)";

// Standard: das bisherige Clipline-Aussehen mit Rahmen
static const char* kStandardSheet = R"(
* { font-family: 'Segoe UI','SF Pro Text','Helvetica Neue','Ubuntu','Noto Sans',sans-serif; font-size: 13px; color: @text@; }
QWidget#root, QDialog, QMainWindow { background: @bg@; }
QLabel { background: transparent; }
QLabel#muted { color: @muted@; }
QLabel#h1 { font-size: 24px; font-weight: 700; }
QLabel#h2 { font-size: 17px; font-weight: 650; }
QLabel#big { font-size: 30px; font-weight: 750; color: @accent@; }
QLabel#pill { background: @panel2@; border: 1px solid @border@; border-radius: 12px; padding: 4px 12px; }
QFrame#card { background: @panel@; border: 1px solid @border@; border-radius: 14px; }
QFrame#side { background: @panel@; border-right: 1px solid @border@; }
QPushButton { background: @panel2@; border: 1px solid @border@; border-radius: 9px; padding: 8px 14px; }
QPushButton:hover { border-color: @accent@; }
QPushButton:pressed { background: @border@; }
QPushButton:disabled { color: @muted@; }
QPushButton#primary { background: @accent@; color: @acctext@; border-color: @accent@; font-weight: 650; }
QPushButton#primary:hover { background: @accent2@; }
QPushButton#flat { background: transparent; border: none; color: @muted@; padding: 6px 8px; }
QPushButton#flat:hover { color: @text@; }
QPushButton#hotkey { font-size: 20px; font-weight: 700; letter-spacing: 1px; padding: 12px; border-radius: 14px;
    border: 2px solid @border@; border-bottom-width: 5px; background: @panel2@; }
QPushButton#hotkey:hover { border-color: @accent@; }
QPushButton#hotkey[recording="true"] { border-color: @accent@; color: @accent@; background: @panel@; }
QPushButton#swatch { border-radius: 15px; min-width: 30px; max-width: 30px; min-height: 30px; max-height: 30px; padding: 0; border: 2px solid transparent; }
QPushButton#swatch:checked { border: 3px solid @text@; }
QPushButton#seg { border-radius: 0; padding: 7px 16px; }
QPushButton#seg:checked { background: @accent@; color: @acctext@; border-color: @accent@; }
QComboBox, QLineEdit, QKeySequenceEdit { background: @panel2@; border: 1px solid @border@; border-radius: 8px; padding: 6px 9px; min-height: 20px; }
QComboBox:hover, QLineEdit:hover { border-color: @accent@; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView { background: @panel@; border: 1px solid @border@; outline: 0; selection-background-color: @accent@; selection-color: @acctext@; }
QCheckBox { spacing: 10px; }
@toggle@
QSlider::groove:horizontal { height: 6px; background: @border@; border-radius: 3px; }
QSlider::sub-page:horizontal { background: @accent@; border-radius: 3px; }
QSlider::handle:horizontal { width: 20px; height: 20px; margin: -7px 0; background: @text@; border-radius: 10px; }
QProgressBar { background: @panel2@; border: none; border-radius: 5px; height: 10px; }
QProgressBar::chunk { background: @accent@; border-radius: 5px; }
QListView { background: transparent; border: none; outline: 0; }
QListWidget { background: @panel2@; border: 1px solid @border@; border-radius: 12px; padding: 4px; }
QListWidget::item { border-radius: 8px; }
QListWidget::item:hover { background: @border@; }
QListWidget::item:selected { background: @accent@; color: @acctext@; }
QScrollBar:vertical { width: 10px; background: transparent; }
QScrollBar::handle:vertical { background: @border@; border-radius: 5px; min-height: 30px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QTabWidget::pane { border: 1px solid @border@; border-radius: 12px; background: @panel@; top: -1px; }
QTabBar::tab { background: transparent; border: none; padding: 9px 16px; color: @muted@; font-weight: 600; }
QTabBar::tab:selected { color: @text@; border-bottom: 2px solid @accent@; }
QMenu { background: @panel@; border: 1px solid @border@; border-radius: 10px; padding: 6px; }
QMenu::item { padding: 7px 22px 7px 12px; border-radius: 6px; }
QMenu::item:selected { background: @panel2@; }
QMenu::separator { height: 1px; background: @border@; margin: 5px 8px; }
QToolTip { background: @panel@; color: @text@; border: 1px solid @border@; padding: 5px; }
QMessageBox { background: @bg@; }
QPushButton#dashed { background: transparent; border: 2px dashed @border@; border-radius: 12px; padding: 11px; color: @muted@; font-weight: 600; }
QPushButton#dashed:hover { border-color: @accent@; color: @text@; }
QPushButton#flat:hover { background: @panel2@; }
)";

// Acryl: wird an den Standard angehängt. Aufklappendes bleibt deckend, das sind eigene Fenster.
static const char* kAcrylSheet = R"(
QDialog, QWidget#mainroot { background: @bg@; border-image: url(@smoke@) 0 0 0 0 stretch stretch; }
QDialog QWidget#root { background: transparent; }
QFrame#card { background: @glass@; border: 1px solid @edge@; border-radius: 14px; }
QFrame#side { background: @glass@; border: none; border-right: 1px solid @edge@; }
QLabel#pill { background: @glass2@; border: 1px solid @edge@; }
QPushButton { background: @glass2@; border: 1px solid @edge@; }
QPushButton:hover { background: @glass3@; border-color: @edge@; }
QPushButton:pressed { background: @glass@; }
QPushButton#primary { background: @accent@; color: @acctext@; border-color: @accent@; }
QPushButton#primary:hover { background: @accent2@; }
QPushButton#flat { background: transparent; border: none; }
QPushButton#flat:hover { background: @glass2@; }
QPushButton#hotkey { background: @well@; border: 2px solid @edge@; border-bottom-width: 5px; }
QPushButton#hotkey:hover { border-color: @accent@; background: @well@; }
QPushButton#hotkey[recording="true"] { border-color: @accent@; color: @accent@; }
QPushButton#seg:checked { background: @accent@; color: @acctext@; border-color: @accent@; }
QPushButton#dashed { background: transparent; border: 2px dashed @edge@; }
QPushButton#dashed:hover { border-color: @accent@; background: transparent; }
QComboBox, QLineEdit, QKeySequenceEdit { background: @well@; border: 1px solid @edge@; }
QComboBox:hover, QLineEdit:hover { border-color: @accent@; }
QComboBox QAbstractItemView { selection-background-color: @hover@; selection-color: @text@; }
QListWidget { background: @well@; border: 1px solid @edge@; }
QListWidget::item:hover { background: @glass2@; }
QListWidget::item:selected { background: @glass3@; color: @text@; }
QTabWidget::pane { border: 1px solid @edge@; background: @glass@; }
QSlider::groove:horizontal { height: 4px; background: @edge@; border-radius: 2px; }
QSlider::handle:horizontal { width: 16px; height: 16px; margin: -6px 0; background: @text@; border-radius: 8px; }
QProgressBar { background: @well@; }
QScrollBar::handle:vertical { background: @glass3@; }
QMenu::item:selected { background: @hover@; }
)";

// Weichzeichnen in laufender Summe: dreimal Kastenfilter in beide Richtungen
static QImage softBlur(QImage img, int r) {
    const int w = img.width(), h = img.height(), c = 2 * r + 1;
    QList<QRgb> line(std::max(w, h));
    auto run = [&](QRgb* first, int n, int stride) {
        for (int i = 0; i < n; ++i) line[i] = first[i * stride];
        int R = 0, G = 0, B = 0;
        for (int i = -r; i <= r; ++i) {
            const QRgb v = line[std::clamp(i, 0, n - 1)];
            R += qRed(v);
            G += qGreen(v);
            B += qBlue(v);
        }
        for (int i = 0; i < n; ++i) {
            first[i * stride] = qRgb((R + r) / c, (G + r) / c, (B + r) / c);
            const QRgb add = line[std::min(i + r + 1, n - 1)], sub = line[std::max(i - r, 0)];
            R += qRed(add) - qRed(sub);
            G += qGreen(add) - qGreen(sub);
            B += qBlue(add) - qBlue(sub);
        }
    };
    auto* px = reinterpret_cast<QRgb*>(img.bits());
    const int stride = int(img.bytesPerLine() / 4);
    for (int pass = 0; pass < 3; ++pass) {
        for (int y = 0; y < h; ++y) run(px + y * stride, w, 1);
        for (int x = 0; x < w; ++x) run(px + x, h, stride);
    }
    return img;
}

// Rauch: weiche Wolken aus nur einer Farbfamilie (der Akzentfarbe), bewusst ohne Korn. Einmal gezeichnet, als Datei fürs Stylesheet.
static QString smokeFile(const QColor& accent, bool dark) {
    int hue = accent.hslHue() < 0 || accent.hslSaturation() < 24 ? 215 : accent.hslHue();
    if (hue >= 28 && hue <= 75) hue = 22;  // Gelb und Orange würden dunkel olivgrün, deshalb warmes Braunrot
    const QString dir = QDir::temp().filePath("sysline-ui");
    QDir().mkpath(dir);
    const QString path = QString("%1/smoke-%2%3.png").arg(dir).arg(hue).arg(dark ? "d" : "l");
    if (QFileInfo::exists(path)) return path;
    const int W = 640, H = 360;
    QImage im(W, H, QImage::Format_RGB32);
    im.fill(dark ? QColor::fromHsl(hue, 22, 38) : QColor::fromHsl(hue, 30, 232));
    QPainter g(&im);
    g.setPen(Qt::NoPen);
    std::mt19937 rnd(11);
    auto pick = [&](int a, int b) { return a + int(rnd() % uint(b - a + 1)); };
    for (int i = 0; i < 11; ++i) {
        const int h2 = (hue + pick(-14, 14) + 360) % 360;
        g.setBrush(dark ? QColor::fromHsl(h2, pick(50, 92), pick(56, 80)) : QColor::fromHsl(h2, pick(50, 90), pick(214, 226)));
        g.drawEllipse(QRect(pick(-80, W), pick(-80, H), pick(120, 350), pick(80, 240)));
    }
    g.end();
    softBlur(im, 55).save(path);
    return path;
}

// Schalter statt Kästchen: an = Akzentfarbe mit Knopf rechts, aus = grau mit Knopf links - auf einen Blick klar.
// Qt-Stylesheets brauchen dafür Bilddateien; die werden passend zu den Farben einmal gezeichnet (@2x für HiDPI).
static QString toggleSheet(const Palette& p) {
    const QString dir = QDir::temp().filePath("sysline-ui");
    QDir().mkpath(dir);
    const QString key = QString("%1%2%3").arg(p.accent.name().mid(1), p.border.name().mid(1), p.panel.name().mid(1));
    auto draw = [&](bool on, bool enabled, int scale) {
        const QString path = QString("%1/toggle-%2-%3%4%5.png").arg(dir, key, on ? "on" : "off", enabled ? "" : "-dis", scale == 2 ? "@2x" : "");
        if (QFileInfo::exists(path)) return;
        QImage img(38 * scale, 22 * scale, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter g(&img);
        g.setRenderHint(QPainter::Antialiasing);
        g.scale(scale, scale);
        g.setOpacity(enabled ? 1.0 : 0.45);
        const QRectF track(1, 2, 36, 18);
        g.setPen(on ? Qt::NoPen : QPen(p.muted, 1.2));
        g.setBrush(on ? p.accent : p.panel);
        g.drawRoundedRect(track, 9, 9);
        g.setPen(Qt::NoPen);
        g.setBrush(on ? p.accentText : p.muted);
        g.drawEllipse(QPointF(on ? 28 : 10, 11), 6.5, 6.5);
        g.end();
        img.save(path);
    };
    for (bool on : {false, true})
        for (bool en : {true, false})
            for (int sc : {1, 2}) draw(on, en, sc);
    auto url = [&](const char* state) { return QString("%1/toggle-%2-%3.png").arg(dir, key, state); };
    return QString("QCheckBox::indicator { width: 38px; height: 22px; border: none; background: transparent; image: url(%1); }\n"
                   "QCheckBox::indicator:checked { image: url(%2); }\n"
                   "QCheckBox::indicator:disabled { image: url(%3); }\n"
                   "QCheckBox::indicator:checked:disabled { image: url(%4); }\n")
        .arg(url("off"), url("on"), url("off-dis"), url("on-dis"));
}

// Sysline: Seitenleiste, Kacheln, Hinweise (gilt für alle Stile)
static const char* kSyslineSheet = R"(
QWidget#mainroot { background: @bg@; }
QStackedWidget, QStackedWidget > QWidget { background: transparent; }
QPushButton#nav { background: transparent; border: none; border-radius: 9px; padding: 9px 12px; text-align: left; color: @muted@; font-weight: 600; font-size: 14px; }
QPushButton#nav:hover { background: @panel2@; color: @text@; }
QPushButton#nav:checked { background: @panel2@; color: @text@; border-left: 3px solid @accent@; }
QLabel#value { font-size: 28px; font-weight: 750; }
QLabel#unit { color: @muted@; font-size: 13px; }
QLabel#tiny { color: @muted@; font-size: 12px; }
QLabel#key { background: @panel2@; border: 1px solid @border@; border-bottom-width: 3px; border-radius: 7px; padding: 4px 10px; font-weight: 700; }
QFrame#warn { background: @warnbg@; border: 1px solid @accent@; border-radius: 12px; }
QFrame#note { background: @panel2@; border: 1px solid @border@; border-radius: 12px; }
QPlainTextEdit { background: @panel2@; border: 1px solid @border@; border-radius: 10px; padding: 8px; font-family: 'Cascadia Mono','JetBrains Mono','SF Mono',Menlo,Consolas,'DejaVu Sans Mono',monospace; font-size: 13px; }
QSpinBox { background: @panel2@; border: 1px solid @border@; border-radius: 8px; padding: 5px 8px; }
QScrollArea { background: transparent; border: none; }
QScrollArea > QWidget > QWidget { background: transparent; }
)";

QString styleSheet(const Palette& p) {
    QString s = QString::fromUtf8(g_minimal && !g_acryl ? kMinimalSheet : kStandardSheet);
    s += QString::fromUtf8(kSyslineSheet);
    {
        QColor w = p.accent;
        w.setAlpha(p.bg.lightness() < 128 ? 38 : 26);
        s.replace("@warnbg@", QString("rgba(%1,%2,%3,%4)").arg(w.red()).arg(w.green()).arg(w.blue()).arg(w.alphaF(), 0, 'f', 2));
    }
    if (g_acryl) {
        const bool dark = p.bg.lightness() < 128;
        s += QString::fromUtf8(kAcrylSheet);
        s.replace("@smoke@", smokeFile(p.accent, dark));
        s.replace("@glass3@", dark ? "rgba(255,255,255,0.16)" : "rgba(255,255,255,0.92)");
        s.replace("@glass2@", dark ? "rgba(255,255,255,0.09)" : "rgba(255,255,255,0.66)");
        s.replace("@glass@", dark ? "rgba(255,255,255,0.055)" : "rgba(255,255,255,0.46)");
        s.replace("@edge@", dark ? "rgba(255,255,255,0.11)" : "rgba(0,0,0,0.10)");
        s.replace("@well@", dark ? "rgba(0,0,0,0.30)" : "rgba(255,255,255,0.56)");
    }
    s.replace("@toggle@", toggleSheet(p));
    s.replace("@bg@", p.bg.name()).replace("@panel@", p.panel.name()).replace("@panel2@", p.panel2.name())
        .replace("@border@", p.border.name()).replace("@text@", p.text.name()).replace("@muted@", p.muted.name())
        .replace("@accent2@", p.accent.lighter(112).name()).replace("@accent@", p.accent.name())
        .replace("@acctext@", p.accentText.name()).replace("@hover@", p.hover.name());
    return s;
}

// ---------------------------------------------------------------- Weiches Scrollen
namespace {
class SmoothScroller : public QObject {
public:
    explicit SmoothScroller(QAbstractScrollArea* area) : QObject(area), area_(area) {
        anim_.setDuration(280);
        anim_.setEasingCurve(QEasingCurve::OutCubic);
        connect(&anim_, &QVariantAnimation::valueChanged, this,
                [this](const QVariant& v) { area_->verticalScrollBar()->setValue(qRound(v.toDouble())); });
        // Wer selbst am Balken zieht, übernimmt sofort
        connect(area->verticalScrollBar(), &QScrollBar::sliderPressed, &anim_, &QVariantAnimation::stop);
        area->viewport()->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject*, QEvent* e) override {
        if (e->type() != QEvent::Wheel) return false;
        auto* w = static_cast<QWheelEvent*>(e);
        // Touchpad (Pixel-Schritte) und Strg+Rad unverändert lassen
        if (!w->pixelDelta().isNull() || w->angleDelta().y() == 0 || (w->modifiers() & Qt::ControlModifier)) return false;
        QScrollBar* sb = area_->verticalScrollBar();
        if (sb->maximum() <= sb->minimum()) return false;
        // Während die Animation noch läuft, vom Ziel aus weiterrechnen - schnelles Drehen scrollt weiter
        const double from = anim_.state() == QAbstractAnimation::Running ? target_ : sb->value();
        const double step = std::max(80.0, area_->viewport()->height() * 0.2);  // pro Rastung
        target_ = std::clamp(from - w->angleDelta().y() / 120.0 * step, double(sb->minimum()), double(sb->maximum()));
        anim_.stop();
        anim_.setStartValue(double(sb->value()));
        anim_.setEndValue(target_);
        anim_.start();
        return true;
    }

private:
    QAbstractScrollArea* area_;
    QVariantAnimation anim_;
    double target_ = 0;
};
}  // namespace

void enableSmoothScroll(QAbstractScrollArea* area) { new SmoothScroller(area); }

// ---------------------------------------------------------------- Kein Verstellen per Mausrad
// Beim Durchscrollen der Einstellungen hat das Mausrad sonst Auswahllisten, Regler und Zahlenfelder verstellt,
// sobald der Zeiger darüber lag. Jetzt scrollt das Rad immer die Seite; verstellt wird nur per Klick/Ziehen.
namespace {
class NoWheelChange : public QObject {
protected:
    bool eventFilter(QObject* o, QEvent* e) override {
        if (e->type() != QEvent::Wheel) return false;
        auto* w = qobject_cast<QWidget*>(o);
        if (!w) return false;
        QWidget* ctl = nullptr;
        for (QWidget* x = w; x; x = x->parentWidget()) {
            if (qobject_cast<QComboBox*>(x) || qobject_cast<QAbstractSpinBox*>(x) ||
                (qobject_cast<QAbstractSlider*>(x) && !qobject_cast<QScrollBar*>(x))) {
                ctl = x;
                break;
            }
            if (x->isWindow()) break;
        }
        if (!ctl) return false;
        // Die aufgeklappte Liste einer Auswahl darf weiter scrollen
        if (w->window() != ctl->window()) return false;
        for (QWidget* x = ctl->parentWidget(); x; x = x->parentWidget()) {
            if (auto* area = qobject_cast<QAbstractScrollArea*>(x)) {
                QCoreApplication::sendEvent(area->viewport(), e);
                return true;
            }
        }
        return true;
    }
};
}  // namespace

void installNoWheelChange() { qApp->installEventFilter(new NoWheelChange); }

// ---------------------------------------------------------------- Logo
// Sysline: Karmin-Kachel, weißer Messbogen mit Zeiger - eine einfache, klare Form, auch in 16 px lesbar
QPixmap syslineLogo(int size) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 100.0, size / 100.0);
    QLinearGradient g(0, 0, 100, 100);
    g.setColorAt(0, QColor("#ef3b5d"));
    g.setColorAt(1, QColor("#9b1030"));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawRoundedRect(QRectF(4, 4, 92, 92), 24, 24);
    QPen pen(Qt::white, 9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(21, 25, 58, 58), -30 * 16, 240 * 16);  // Bogen, unten offen
    // Zeiger nach rechts oben
    const double a = 40 * 3.14159265 / 180;
    p.drawLine(QPointF(50, 54), QPointF(50 + 22 * std::cos(a), 54 - 22 * std::sin(a)));
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawEllipse(QPointF(50, 54), 7.5, 7.5);
    return pm;
}

QIcon syslineIcon() {
    QIcon ic;
    for (int s : {16, 24, 32, 48, 64, 128, 256}) ic.addPixmap(syslineLogo(s));
    return ic;
}

// ---------------------------------------------------------------- UI-Symbole
QIcon icon(Ic id, const QColor& c) {
    const int S = 64;
    QPixmap pm(S, S);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(S / 100.0, S / 100.0);
    p.setPen(QPen(c, 8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    auto fill = [&] { p.setPen(Qt::NoPen); p.setBrush(c); };
    switch (id) {
    case Ic::Gauge:
        p.drawArc(QRectF(14, 20, 72, 72), -20 * 16, 220 * 16);
        p.drawLine(QPointF(50, 56), QPointF(70, 36));
        break;
    case Ic::Overlay:
        p.drawRoundedRect(QRectF(10, 18, 80, 64), 10, 10);
        fill();
        p.drawRoundedRect(QRectF(20, 28, 28, 18), 4, 4);
        break;
    case Ic::Keys:
        p.drawRoundedRect(QRectF(8, 24, 84, 52), 10, 10);
        for (int x : {24, 42, 60, 78}) p.drawPoint(QPointF(x - 2, 42));
        p.drawLine(QPointF(30, 60), QPointF(70, 60));
        break;
    case Ic::Macro:
        p.drawLine(QPointF(14, 28), QPointF(54, 28));
        p.drawLine(QPointF(14, 50), QPointF(54, 50));
        p.drawLine(QPointF(14, 72), QPointF(40, 72));
        fill();
        {
            QPainterPath t; t.moveTo(62, 52); t.lineTo(90, 70); t.lineTo(62, 88); t.closeSubpath();
            p.drawPath(t);
        }
        break;
    case Ic::Profile:
        p.drawEllipse(QPointF(50, 34), 16, 16);
        p.drawArc(QRectF(18, 56, 64, 56), 0, 180 * 16);
        break;
    case Ic::Gear:
        p.drawEllipse(QPointF(50, 50), 25, 25);
        p.drawEllipse(QPointF(50, 50), 9, 9);
        for (int i = 0; i < 8; ++i) {
            p.save(); p.translate(50, 50); p.rotate(i * 45); p.drawLine(0, -25, 0, -39); p.restore();
        }
        break;
    case Ic::Trash: {
        p.drawLine(16, 28, 84, 28); p.drawLine(38, 28, 38, 16); p.drawLine(38, 16, 62, 16); p.drawLine(62, 16, 62, 28);
        QPainterPath b; b.moveTo(24, 28); b.lineTo(29, 84); b.lineTo(71, 84); b.lineTo(76, 28);
        p.drawPath(b);
        break;
    }
    case Ic::Edit:
        p.drawLine(24, 76, 34, 52); p.drawLine(34, 52, 68, 18); p.drawLine(68, 18, 82, 32);
        p.drawLine(82, 32, 48, 66); p.drawLine(48, 66, 24, 76);
        break;
    case Ic::Record:
        p.drawEllipse(QPointF(50, 50), 36, 36);
        fill();
        p.drawEllipse(QPointF(50, 50), 19, 19);
        break;
    case Ic::Arrow:
        p.drawLine(QPointF(14, 50), QPointF(84, 50));
        p.drawLine(QPointF(62, 28), QPointF(84, 50));
        p.drawLine(QPointF(62, 72), QPointF(84, 50));
        break;
    }
    p.end();
    return QIcon(pm);
}
