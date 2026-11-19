#pragma once
#include <QColor>
#include <QList>
#include <QRect>
#include <QString>
#include <QTimer>
#include <QWidget>
#include "config.h"
#include "sensors.h"

// Eine Zeile im Overlay ("GPU" | "97 %  64 °C")
struct OverlayRow {
    QString label;
    QString value;
    bool hot = false;  // heiß / hoher Ping: in der Akzentfarbe
};

// Reine Funktionen (Selbsttest): welche Zeilen, wie groß, wo auf dem Bildschirm
QList<OverlayRow> overlayRows(const Metrics& m, const OverlayOptions& o, const QTime& now);
QSize overlaySize(const QList<OverlayRow>& rows, const OverlayOptions& o);
QRect placeOverlay(const QSize& size, const QRect& screen, Corner corner, int margin);
void paintOverlay(QPainter& p, const QPoint& topLeft, const QList<OverlayRow>& rows, const OverlayOptions& o, const QColor& accent);
QString fmtNumber(double v, int decimals);

// Das eigentliche Overlay: rahmenlos, durchsichtig, durchklickbar, immer vorne
class OverlayWindow : public QWidget {
public:
    explicit OverlayWindow(QWidget* parent = nullptr);
    void setOptions(const OverlayOptions& o, const QColor& accent);
    void setMetrics(const Metrics& m);
    QImage render() const;  // Selbsttest/Website

protected:
    void paintEvent(QPaintEvent*) override;
    void showEvent(QShowEvent*) override;

private:
    void relayout();
    OverlayOptions opt_;
    QColor accent_;
    Metrics m_;
    QList<OverlayRow> rows_;
    QTimer topTimer_;
};

// Vorschau in den Einstellungen: gezeichnete Spielszene mit dem Overlay in der gewählten Ecke
class OverlayPreview : public QWidget {
public:
    explicit OverlayPreview(QWidget* parent = nullptr);
    void setOptions(const OverlayOptions& o, const QColor& accent);
    void setMetrics(const Metrics& m);
    QSize sizeHint() const override { return {520, 293}; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    OverlayOptions opt_;
    QColor accent_;
    Metrics m_;
};
void paintGameScene(QPainter& p, const QRectF& r);
