#pragma once
#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

// Farben + Stylesheet wie in den anderen Line-Programmen (Akzentfarbe, Hell/Dunkel, Stile Standard/Minimal/Acryl)
struct Palette {
    QColor bg, panel, panel2, border, text, muted, accent, accentText, hover;
};
Palette makePalette(const QString& accent, bool dark);
QString styleSheet(const Palette& p);
void setMinimalLook(bool on);
bool minimalLook();
void setAcrylLook(bool on);
bool acrylLook();
const QList<QColor>& accentChoices();

QPixmap syslineLogo(int size);
QIcon syslineIcon();

enum class Ic { Gauge, Overlay, Keys, Macro, Profile, Gear, Trash, Edit, Record, Arrow };
QIcon icon(Ic id, const QColor& c);

class QAbstractScrollArea;
void enableSmoothScroll(QAbstractScrollArea* area);
void installNoWheelChange();
