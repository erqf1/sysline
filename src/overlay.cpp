#include "overlay.h"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QScreen>
#include <QTime>
#include <algorithm>
#include <cmath>
#include "i18n.h"
#include "platform.h"

QString fmtNumber(double v, int decimals) {
    const QLocale loc(language() == "de" ? QLocale::German : QLocale::English);
    return loc.toString(v, 'f', decimals);
}

static QString pct(double v) { return QString("%1 %").arg(int(std::lround(v))); }
static QString deg(double v) { return QString("%1 °C").arg(int(std::lround(v))); }
static QString gib(double used, double total) {
    return QString("%1 / %2 GB").arg(fmtNumber(used, 1), fmtNumber(total, total >= 10 ? 0 : 1));
}

QList<OverlayRow> overlayRows(const Metrics& m, const OverlayOptions& o, const QTime& now) {
    QList<OverlayRow> rows;
    if (o.on(Item::Fps)) {
        QString v;
        switch (m.fpsState) {
        case FpsState::Ok: v = QString::number(int(std::lround(m.fps))); break;
        case FpsState::NeedsAdmin: v = L("admin rights needed"); break;
        case FpsState::Unsupported: v = L("Windows only"); break;
        case FpsState::Starting: v = "…"; break;
        case FpsState::NoFrames: v = "–"; break;
        }
        rows << OverlayRow{"FPS", v, false};
    }
    auto loadTemp = [&](const char* label, bool loadOn, double load, bool tempOn, double temp, double hotAt) {
        QStringList parts;
        if (loadOn) parts << (load >= 0 ? pct(load) : QString("–"));
        if (tempOn && temp > 0) parts << deg(temp);
        if (parts.isEmpty()) return;
        rows << OverlayRow{label, parts.join("  "), tempOn && temp >= hotAt};
    };
    loadTemp("CPU", o.on(Item::Cpu), m.cpu, o.on(Item::CpuTemp), m.cpuTemp, 90);
    loadTemp("GPU", o.on(Item::Gpu), m.gpu, o.on(Item::GpuTemp), m.gpuTemp, 85);
    if (o.on(Item::Ram)) rows << OverlayRow{"RAM", m.ramTotal > 0 ? gib(m.ramUsed, m.ramTotal) : QString("–"), false};
    if (o.on(Item::Vram)) rows << OverlayRow{"VRAM", m.vramTotal > 0 ? gib(m.vramUsed, m.vramTotal) : QString("–"), false};
    if (o.on(Item::Ping))
        rows << OverlayRow{"PING", m.ping >= 0 ? QString("%1 ms").arg(m.ping < 1 ? QString("<1") : QString::number(int(std::lround(m.ping))))
                                               : QString("–"),
                           m.ping >= 100};
    if (o.on(Item::Clock)) rows << OverlayRow{L("TIME"), now.toString("HH:mm"), false};
    if (o.on(Item::Battery) && m.battery >= 0)
        rows << OverlayRow{L("BATT"), QString("%1 %%2").arg(m.battery).arg(m.charging ? QString(" +") : QString()), m.battery <= 15 && !m.charging};
    return rows;
}

namespace {
struct Fonts {
    QFont label, value;
    int pad, gap, rowGap, colGap;
};
Fonts fontsFor(const OverlayOptions& o) {
    const double s = o.scale / 100.0;
    Fonts f;
    f.label = QFont(QStringList{"Segoe UI", "Inter", "SF Pro Text", "Noto Sans", "DejaVu Sans"});
    f.label.setPixelSize(std::max(8, int(std::lround(11 * s))));
    f.label.setWeight(QFont::DemiBold);
    f.label.setLetterSpacing(QFont::PercentageSpacing, 104);
    f.value = f.label;
    f.value.setPixelSize(std::max(9, int(std::lround(15 * s))));
    f.value.setWeight(QFont::Bold);
    f.value.setLetterSpacing(QFont::PercentageSpacing, 100);
    f.pad = int(std::lround(10 * s));
    f.gap = int(std::lround(12 * s));     // Abstand Beschriftung - Wert
    f.rowGap = int(std::lround(3 * s));
    f.colGap = int(std::lround(18 * s));  // Abstand zwischen Einträgen in einer Zeile
    return f;
}
}  // namespace

QSize overlaySize(const QList<OverlayRow>& rows, const OverlayOptions& o) {
    if (rows.isEmpty()) return QSize(0, 0);
    const Fonts f = fontsFor(o);
    const QFontMetrics lm(f.label), vm(f.value);
    const int rowH = std::max(lm.height(), vm.height());
    if (o.horizontal) {
        int w = 0;
        for (const OverlayRow& r : rows) w += lm.horizontalAdvance(r.label) + f.gap / 2 + vm.horizontalAdvance(r.value);
        w += f.colGap * int(rows.size() - 1);
        return QSize(w + 2 * f.pad, rowH + 2 * f.pad);
    }
    int lw = 0, vw = 0;
    for (const OverlayRow& r : rows) {
        lw = std::max(lw, lm.horizontalAdvance(r.label));
        vw = std::max(vw, vm.horizontalAdvance(r.value));
    }
    const int n = int(rows.size());
    return QSize(lw + f.gap + vw + 2 * f.pad, n * rowH + (n - 1) * f.rowGap + 2 * f.pad);
}

QRect placeOverlay(const QSize& size, const QRect& screen, Corner corner, int margin) {
    const bool right = corner == Corner::TopRight || corner == Corner::BottomRight;
    const bool bottom = corner == Corner::BottomLeft || corner == Corner::BottomRight;
    const int x = right ? screen.right() + 1 - margin - size.width() : screen.left() + margin;
    const int y = bottom ? screen.bottom() + 1 - margin - size.height() : screen.top() + margin;
    QRect r(QPoint(x, y), size);
    // Nie über den Bildschirm hinaus (zu großer Rand oder winziger Bildschirm)
    r.moveLeft(std::clamp(r.left(), screen.left(), std::max(screen.left(), screen.right() + 1 - size.width())));
    r.moveTop(std::clamp(r.top(), screen.top(), std::max(screen.top(), screen.bottom() + 1 - size.height())));
    return r;
}

void paintOverlay(QPainter& p, const QPoint& tl, const QList<OverlayRow>& rows, const OverlayOptions& o, const QColor& accent) {
    if (rows.isEmpty()) return;
    const Fonts f = fontsFor(o);
    const QSize size = overlaySize(rows, o);
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const QRectF box(tl, size);
    if (o.opacity > 0) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(12, 12, 16, int(o.opacity * 2.55)));
        p.drawRoundedRect(box, 8 * o.scale / 100.0, 8 * o.scale / 100.0);
        // dünne Akzentlinie links: zeigt, dass es Sysline ist, ohne zu stören
        p.setBrush(accent);
        const double lw = std::max(2.0, 3 * o.scale / 100.0);
        QPainterPath clip;
        clip.addRoundedRect(box, 8 * o.scale / 100.0, 8 * o.scale / 100.0);
        p.setClipPath(clip);
        p.drawRect(QRectF(box.left(), box.top(), lw, box.height()));
        p.setClipping(false);
    }
    const QFontMetrics lm(f.label), vm(f.value);
    const int rowH = std::max(lm.height(), vm.height());
    const QColor labelC(200, 202, 212), valueC(250, 250, 252);
    const QColor hotC = accent.lightnessF() < 0.55 ? accent.lighter(150) : accent;
    auto text = [&](const QFont& font, const QRect& r, int align, const QString& s, const QColor& c) {
        p.setFont(font);
        if (o.opacity < 35) {  // ohne Hintergrund: Schatten, damit es auf hellen Szenen lesbar bleibt
            p.setPen(QColor(0, 0, 0, 200));
            p.drawText(r.translated(1, 1), align, s);
        }
        p.setPen(c);
        p.drawText(r, align, s);
    };
    if (o.horizontal) {
        int x = tl.x() + f.pad;
        const int y = tl.y() + f.pad;
        for (const OverlayRow& r : rows) {
            const int lw = lm.horizontalAdvance(r.label), vw = vm.horizontalAdvance(r.value);
            text(f.label, QRect(x, y, lw, rowH), Qt::AlignLeft | Qt::AlignVCenter, r.label, labelC);
            x += lw + f.gap / 2;
            text(f.value, QRect(x, y, vw, rowH), Qt::AlignLeft | Qt::AlignVCenter, r.value, r.hot ? hotC : valueC);
            x += vw + f.colGap;
        }
    } else {
        int lwMax = 0;
        for (const OverlayRow& r : rows) lwMax = std::max(lwMax, lm.horizontalAdvance(r.label));
        int y = tl.y() + f.pad;
        const int vx = tl.x() + f.pad + lwMax + f.gap, vw = size.width() - 2 * f.pad - lwMax - f.gap;
        for (const OverlayRow& r : rows) {
            text(f.label, QRect(tl.x() + f.pad, y, lwMax, rowH), Qt::AlignLeft | Qt::AlignVCenter, r.label, labelC);
            text(f.value, QRect(vx, y, vw, rowH), Qt::AlignRight | Qt::AlignVCenter, r.value, r.hot ? hotC : valueC);
            y += rowH + f.rowGap;
        }
    }
    p.restore();
}

// ---------------------------------------------------------------- Fenster
OverlayWindow::OverlayWindow(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::WindowTransparentForInput |
                   Qt::WindowDoesNotAcceptFocus | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setWindowTitle("Sysline Overlay");
    topTimer_.setInterval(2000);
    connect(&topTimer_, &QTimer::timeout, this, [this] { if (isVisible()) keepOverlayOnTop(this); });
}

void OverlayWindow::setOptions(const OverlayOptions& o, const QColor& accent) {
    const bool captureChanged = o.hideFromCapture != opt_.hideFromCapture;
    opt_ = o;
    accent_ = accent;
    relayout();
    if (captureChanged && isVisible()) prepareOverlayWindow(this, opt_.hideFromCapture);
}

void OverlayWindow::setMetrics(const Metrics& m) {
    m_ = m;
    relayout();
}

void OverlayWindow::relayout() {
    rows_ = overlayRows(m_, opt_, QTime::currentTime());
    QScreen* scr = QGuiApplication::primaryScreen();
    for (QScreen* s : QGuiApplication::screens())
        if (!opt_.screen.isEmpty() && s->name() == opt_.screen) scr = s;
    const QSize sz = overlaySize(rows_, opt_).expandedTo(QSize(1, 1));
    const QRect r = scr ? placeOverlay(sz, scr->geometry(), opt_.corner, opt_.margin) : QRect(QPoint(0, 0), sz);
    if (geometry() != r) setGeometry(r);
    update();
}

QImage OverlayWindow::render() const {
    QImage img(overlaySize(rows_, opt_).expandedTo(QSize(1, 1)), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    paintOverlay(p, QPoint(0, 0), rows_, opt_, accent_);
    return img;
}

void OverlayWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(rect(), Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    paintOverlay(p, QPoint(0, 0), rows_, opt_, accent_);
}

void OverlayWindow::showEvent(QShowEvent*) {
    prepareOverlayWindow(this, opt_.hideFromCapture);
    topTimer_.start();
}

// ---------------------------------------------------------------- Vorschau
// Eine schlichte, selbst gezeichnete Landschaft (Abendhimmel, Berge, Boden) - nur damit man das Overlay
// über einem Bild sieht, kein echtes Spiel
void paintGameScene(QPainter& p, const QRectF& r) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient sky(r.topLeft(), r.bottomLeft());
    sky.setColorAt(0, QColor("#1b2140"));
    sky.setColorAt(0.55, QColor("#5a3a63"));
    sky.setColorAt(0.75, QColor("#c26a6a"));
    sky.setColorAt(1, QColor("#2a1d2b"));
    p.fillRect(r, sky);
    QRadialGradient sun(QPointF(r.left() + r.width() * 0.68, r.top() + r.height() * 0.62), r.height() * 0.45);
    sun.setColorAt(0, QColor(255, 214, 190, 200));
    sun.setColorAt(0.15, QColor(255, 170, 150, 120));
    sun.setColorAt(1, QColor(255, 140, 140, 0));
    p.fillRect(r, sun);
    auto ridge = [&](double base, double amp, double freq, double phase, const QColor& c) {
        QPainterPath path;
        path.moveTo(r.left(), r.bottom());
        for (int i = 0; i <= 80; ++i) {
            const double x = r.left() + r.width() * i / 80.0;
            const double t = i / 80.0 * freq + phase;
            const double y = r.top() + r.height() * (base - amp * (0.6 * std::sin(t) + 0.3 * std::sin(t * 2.3 + 1) + 0.1 * std::sin(t * 5.1)));
            path.lineTo(x, y);
        }
        path.lineTo(r.right(), r.bottom());
        path.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPath(path);
    };
    ridge(0.66, 0.12, 7, 0.4, QColor("#3b2a4a"));
    ridge(0.75, 0.10, 9, 2.0, QColor("#291e36"));
    ridge(0.86, 0.06, 13, 4.0, QColor("#17121f"));
    // Fadenkreuz in der Mitte
    const QPointF c = r.center();
    p.setPen(QPen(QColor(255, 255, 255, 190), std::max(1.5, r.height() / 260.0)));
    const double s = r.height() * 0.025;
    p.drawLine(QPointF(c.x() - 2.2 * s, c.y()), QPointF(c.x() - s, c.y()));
    p.drawLine(QPointF(c.x() + s, c.y()), QPointF(c.x() + 2.2 * s, c.y()));
    p.drawLine(QPointF(c.x(), c.y() - 2.2 * s), QPointF(c.x(), c.y() - s));
    p.drawLine(QPointF(c.x(), c.y() + s), QPointF(c.x(), c.y() + 2.2 * s));
    p.restore();
}

OverlayPreview::OverlayPreview(QWidget* parent) : QWidget(parent) {
    setMinimumSize(320, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void OverlayPreview::setOptions(const OverlayOptions& o, const QColor& accent) {
    opt_ = o;
    accent_ = accent;
    update();
}

void OverlayPreview::setMetrics(const Metrics& m) {
    m_ = m;
    update();
}

void OverlayPreview::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QRectF r = rect().adjusted(1, 1, -1, -1);
    if (r.width() / r.height() > 16.0 / 9) r.setWidth(r.height() * 16 / 9);
    else r.setHeight(r.width() * 9 / 16);
    r.moveTopLeft(QPointF(1, 1));
    QPainterPath clip;
    clip.addRoundedRect(r, 12, 12);
    p.setClipPath(clip);
    // Verkleinert wie auf einem 450 px hohen Bildschirm, damit man das Overlay in der Vorschau gut lesen kann
    const double k = r.height() / 450.0;
    p.save();
    p.translate(r.topLeft());
    p.scale(k, k);
    const QRectF scene(0, 0, 800, 450);
    paintGameScene(p, scene);
    OverlayOptions o = opt_;
    const QList<OverlayRow> rows = overlayRows(m_, o, QTime::currentTime());
    const QRect place = placeOverlay(overlaySize(rows, o), scene.toRect(), o.corner, std::max(o.margin, 14));
    paintOverlay(p, place.topLeft(), rows, o, accent_);
    p.restore();
    p.setClipping(false);
    p.setPen(QPen(QColor(255, 255, 255, 30), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r, 12, 12);
}
