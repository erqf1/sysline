#pragma once
#include <QButtonGroup>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <deque>
#include "keys.h"

// Knopf, der beim Anklicken die nächste Tastenkombination aufnimmt (für die Overlay-Taste, Qt-Schreibweise)
class HotkeyButton : public QPushButton {
    Q_OBJECT
public:
    explicit HotkeyButton(const QString& seq, QWidget* parent = nullptr);
    QString sequence() const { return seq_; }
    void setSequence(const QString& seq);
    void pressForTest(int key, Qt::KeyboardModifiers mods);

signals:
    void sequenceChanged(const QString& seq);

protected:
    bool event(QEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;

private:
    void setRecording(bool on);
    void updateText();
    QString seq_;
    bool recording_ = false;
    QTimer hint_;
};

// Knopf für eine einzelne Taste oder (combo = true) eine Kombination wie Strg+C. Klick = nächste Taste aufnehmen,
// Pfeil daneben = aus einer Liste wählen (auch Tasten, die man gerade nicht drücken kann, z. B. "abschalten").
class KeyButton : public QPushButton {
    Q_OBJECT
public:
    KeyButton(const KeyCombo& c, bool combo, QWidget* parent = nullptr);
    KeyCombo combo() const { return c_; }
    void setCombo(const KeyCombo& c);
    void pressForTest(int qtKey, Qt::KeyboardModifiers mods, quint32 nativeVk = 0);

signals:
    void comboChanged(const KeyCombo& c);

protected:
    bool event(QEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

private:
    void setRecording(bool on);
    void updateText();
    void take(int vkCode, Qt::KeyboardModifiers mods);
    void showList();
    KeyCombo c_;
    bool comboMode_;
    bool recording_ = false;
};

// Kleiner Verlauf der letzten Werte
class Sparkline : public QWidget {
public:
    explicit Sparkline(QWidget* parent = nullptr);
    void add(double v);  // < 0 = Lücke
    void setRange(double max) { max_ = max; }
    void setColor(const QColor& c) { color_ = c; update(); }
    void fillDemo(double base, double spread, int seed);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    std::deque<double> v_;
    double max_ = 100;
    QColor color_;
};

// Hilfen für die Seiten
QString hotkeyText(const QString& seq);  // "Strg+Umschalt+O" auf Deutsch
QLabel* mkLabel(const QString& text, const char* objectName = nullptr, bool wrap = true);
QWidget* mkSegmented(const QStringList& names, int current, QButtonGroup** out);
QCheckBox* mkToggle(const QString& text, bool on);
QFrame* mkCard(QVBoxLayout** out, const char* objectName = "card");
