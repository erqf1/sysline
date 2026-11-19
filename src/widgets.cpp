#include "widgets.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <algorithm>
#include <cmath>
#include "i18n.h"

// ---------------------------------------------------------------- Hilfen
QLabel* mkLabel(const QString& text, const char* objectName, bool wrap) {
    auto* l = new QLabel(text);
    if (objectName) l->setObjectName(objectName);
    l->setWordWrap(wrap);
    return l;
}

QWidget* mkSegmented(const QStringList& names, int current, QButtonGroup** out) {
    auto* seg = new QWidget;
    auto* sl = new QHBoxLayout(seg);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(0);
    auto* group = new QButtonGroup(seg);
    for (int q = 0; q < names.size(); ++q) {
        auto* b = new QPushButton(names[q]);
        b->setObjectName("seg");
        b->setCheckable(true);
        b->setChecked(current == q);
        b->setCursor(Qt::PointingHandCursor);
        group->addButton(b, q);
        sl->addWidget(b);
    }
    sl->addStretch();
    *out = group;
    return seg;
}

QCheckBox* mkToggle(const QString& text, bool on) {
    auto* c = new QCheckBox(text);
    c->setChecked(on);
    c->setCursor(Qt::PointingHandCursor);
    return c;
}

QFrame* mkCard(QVBoxLayout** out, const char* objectName) {
    auto* card = new QFrame;
    card->setObjectName(objectName);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(18, 14, 18, 16);
    v->setSpacing(8);
    *out = v;
    return card;
}

static void repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

QString hotkeyText(const QString& seq) {
    QString t = QKeySequence(seq).toString(QKeySequence::NativeText);
    if (language() == "de") t.replace("Ctrl", "Strg").replace("Shift", "Umschalt");
    return t;
}

// ---------------------------------------------------------------- Overlay-Taste
HotkeyButton::HotkeyButton(const QString& seq, QWidget* parent) : QPushButton(parent), seq_(seq) {
    setObjectName("hotkey");
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(48);
    hint_.setSingleShot(true);
    connect(&hint_, &QTimer::timeout, this, [this] { updateText(); });
    connect(this, &QPushButton::clicked, this, [this] { setRecording(!recording_); });
    updateText();
}

void HotkeyButton::setSequence(const QString& seq) {
    seq_ = seq;
    setRecording(false);
}

void HotkeyButton::setRecording(bool on) {
    recording_ = on;
    setProperty("recording", on);
    repolish(this);
    if (on) {
        setFocus();
        grabKeyboard();
    } else {
        releaseKeyboard();
    }
    updateText();
}

void HotkeyButton::updateText() {
    if (hint_.isActive()) return;
    if (recording_) return setText(L("Press keys…"));
    const QString native = hotkeyText(seq_);
    setText(native.isEmpty() ? "—" : native.split('+').join("  +  "));
}

bool HotkeyButton::event(QEvent* e) {
    if (recording_ && e->type() == QEvent::ShortcutOverride) {
        e->accept();
        return true;
    }
    if (recording_ && e->type() == QEvent::KeyPress) {
        keyPressEvent(static_cast<QKeyEvent*>(e));
        return true;
    }
    return QPushButton::event(e);
}

void HotkeyButton::keyPressEvent(QKeyEvent* e) {
    if (!recording_) return QPushButton::keyPressEvent(e);
    pressForTest(e->key(), e->modifiers());
}

void HotkeyButton::pressForTest(int key, Qt::KeyboardModifiers mods) {
    const Qt::KeyboardModifiers real = mods & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier);
    if (key == Qt::Key_Escape && !real) return setRecording(false);
    if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta || key == 0 ||
        key == Qt::Key_unknown)
        return;
    const bool fkey = key >= Qt::Key_F1 && key <= Qt::Key_F24;
    if (!real && !fkey && key != Qt::Key_Pause && key != Qt::Key_ScrollLock) {
        // Buchstaben, Enter usw. allein könnte man sonst nirgends mehr tippen
        hint_.start(2200);
        setText(L("Only together with Ctrl, Alt or Shift"));
        return;
    }
    seq_ = QKeySequence(QKeyCombination(real, Qt::Key(key))).toString(QKeySequence::PortableText);
    setRecording(false);
    emit sequenceChanged(seq_);
}

void HotkeyButton::focusOutEvent(QFocusEvent* e) {
    if (recording_) setRecording(false);
    QPushButton::focusOutEvent(e);
}

// ---------------------------------------------------------------- Taste / Kombination
KeyButton::KeyButton(const KeyCombo& c, bool combo, QWidget* parent) : QPushButton(parent), c_(c), comboMode_(combo) {
    setObjectName("hotkey");
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(42);
    setMinimumWidth(130);
    setStyleSheet("QPushButton#hotkey { font-size: 15px; padding: 6px 12px; }");
    setToolTip(L("Click and press a key. Right-click to pick from a list."));
    connect(this, &QPushButton::clicked, this, [this] { setRecording(!recording_); });
    updateText();
}

void KeyButton::setCombo(const KeyCombo& c) {
    c_ = c;
    setRecording(false);
}

void KeyButton::setRecording(bool on) {
    recording_ = on;
    setProperty("recording", on);
    repolish(this);
    if (on) {
        setFocus();
        grabKeyboard();
    } else {
        releaseKeyboard();
    }
    updateText();
}

void KeyButton::updateText() {
    if (recording_) return setText(L("Press a key…"));
    setText(c_.key || !c_.mods.isEmpty() ? comboLabel(c_) : (comboMode_ ? L("(off)") : QString("—")));
}

bool KeyButton::event(QEvent* e) {
    if (recording_ && e->type() == QEvent::ShortcutOverride) {
        e->accept();
        return true;
    }
    if (recording_ && e->type() == QEvent::KeyPress) {
        keyPressEvent(static_cast<QKeyEvent*>(e));
        return true;
    }
    return QPushButton::event(e);
}

void KeyButton::keyPressEvent(QKeyEvent* e) {
    if (!recording_) return QPushButton::keyPressEvent(e);
    pressForTest(e->key(), e->modifiers(), e->nativeVirtualKey());
}

void KeyButton::pressForTest(int qtKey, Qt::KeyboardModifiers mods, quint32 nativeVk) {
    int code = 0;
#ifdef Q_OS_WIN
    if (nativeVk > 0 && nativeVk < 256) code = int(nativeVk);  // genau die Taste, auch Sonderzeichen jeder Belegung
#else
    Q_UNUSED(nativeVk);
#endif
    if (!code) code = keyFromQt(qtKey, mods & Qt::KeypadModifier);
    if (!code) return;
    take(code, mods);
}

void KeyButton::take(int code, Qt::KeyboardModifiers mods) {
    KeyCombo c;
    if (comboMode_ && !isModifierKey(code)) {
        if (mods & Qt::ControlModifier) c.mods << vk::Ctrl;
        if (mods & Qt::ShiftModifier) c.mods << vk::Shift;
        if (mods & Qt::AltModifier) c.mods << vk::Alt;
        if (mods & Qt::MetaModifier) c.mods << vk::LWin;
    }
    c.key = code;
    c_ = c;
    setRecording(false);
    emit comboChanged(c_);
}

void KeyButton::contextMenuEvent(QContextMenuEvent* e) {
    e->accept();
    showList();
}

void KeyButton::showList() {
    setRecording(false);
    QMenu menu(this);
    auto add = [&](QMenu* m, int code) {
        m->addAction(keyLabel(code), this, [this, code] {
            KeyCombo c;
            c.key = code;
            c_ = c;
            updateText();
            emit comboChanged(c_);
        });
    };
    if (comboMode_)
        menu.addAction(L("Switch key off"), this, [this] {
            c_ = KeyCombo{};
            updateText();
            emit comboChanged(c_);
        });
    QMenu* letters = menu.addMenu(L("Letters and digits"));
    QMenu* fkeys = menu.addMenu(L("Function keys"));
    QMenu* mods = menu.addMenu(L("Shift, Ctrl, Alt, Win"));
    QMenu* nav = menu.addMenu(L("Navigation and editing"));
    QMenu* num = menu.addMenu(L("Number pad"));
    QMenu* media = menu.addMenu(L("Media and volume"));
    QMenu* other = menu.addMenu(L("Other"));
    for (int k : allKeys()) {
        if ((k >= vk::KeyA && k < vk::KeyA + 26) || (k >= vk::Key0 && k < vk::Key0 + 10)) add(letters, k);
        else if (k >= vk::F1 && k <= vk::F24) add(fkeys, k);
        else if (isModifierKey(k)) add(mods, k);
        else if ((k >= vk::Num0 && k <= vk::NumDiv) || k == vk::NumLock) add(num, k);
        else if (k >= vk::VolMute && k <= vk::MediaPlay) add(media, k);
        else if ((k >= vk::PageUp && k <= vk::Down) || k == vk::Insert || k == vk::Delete || k == vk::Backspace ||
                 k == vk::Enter || k == vk::Tab || k == vk::Space || k == vk::Esc)
            add(nav, k);
        else add(other, k);
    }
    menu.exec(mapToGlobal(QPoint(0, height())));
}

void KeyButton::focusOutEvent(QFocusEvent* e) {
    if (recording_) setRecording(false);
    QPushButton::focusOutEvent(e);
}

// ---------------------------------------------------------------- Verlauf
Sparkline::Sparkline(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(34);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void Sparkline::add(double v) {
    v_.push_back(v);
    while (v_.size() > 60) v_.pop_front();
    update();
}

void Sparkline::fillDemo(double base, double spread, int seed) {
    v_.clear();
    for (int i = 0; i < 60; ++i)
        v_.push_back(std::max(0.0, base + spread * (0.6 * std::sin(i * 0.31 + seed) + 0.4 * std::sin(i * 0.83 + seed * 2.1))));
    update();
}

void Sparkline::paintEvent(QPaintEvent*) {
    if (v_.size() < 2) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(1, 3, -1, -1);
    // Bereich passt sich an: kleine Schwankungen sollen sichtbar sein
    double lo = 1e9, hi = -1e9;
    for (double v : v_)
        if (v >= 0) { lo = std::min(lo, v); hi = std::max(hi, v); }
    if (hi < 0) return;
    const double span = std::max(hi - lo, std::max(1.0, max_ * 0.05));
    lo = std::max(0.0, lo - span * 0.6);
    hi = hi + span * 0.4;
    const int n = 60;
    auto pt = [&](int i, double v) {
        return QPointF(r.left() + r.width() * (n - int(v_.size()) + i) / (n - 1), r.bottom() - r.height() * (v - lo) / (hi - lo));
    };
    QPainterPath line, area;
    bool open = false;
    for (int i = 0; i < int(v_.size()); ++i) {
        if (v_[size_t(i)] < 0) { open = false; continue; }
        const QPointF q = pt(i, v_[size_t(i)]);
        if (!open) line.moveTo(q); else line.lineTo(q);
        open = true;
    }
    area = line;
    if (!line.isEmpty()) {
        area.lineTo(line.currentPosition().x(), r.bottom());
        area.lineTo(line.elementAt(0).x, r.bottom());
        area.closeSubpath();
    }
    QColor fillC = color_;
    fillC.setAlpha(40);
    p.setPen(Qt::NoPen);
    p.setBrush(fillC);
    p.drawPath(area);
    p.setPen(QPen(color_, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPath(line);
}
