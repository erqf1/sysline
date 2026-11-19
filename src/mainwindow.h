#pragma once
#include <QComboBox>
#include <QDialog>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QWidget>
#include "engine.h"
#include "sensors.h"

class App;
class OverlayPreview;
class Sparkline;
class KeyButton;
class QButtonGroup;

enum Page { PageOverview, PageOverlay, PageKeys, PageMacros, PageProfiles, PageSettings, PageCount };

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(App* app);
    void showPage(int i);
    void rebuild();  // nach Sprach- oder Stilwechsel alles neu aufbauen
    QStackedWidget* stack() const { return stack_; }
    void setEditProfile(int i);
    // Website-Bilder: Verläufe mit gleichmäßigen Beispielwerten statt der kurzen Testmessung
    void fillDemoHistory();

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    QWidget* wrap(QWidget* content);
    QWidget* overviewPage();
    QWidget* overlayPage();
    QWidget* keysPage();
    QWidget* macrosPage();
    QWidget* profilesPage();
    QWidget* settingsPage();
    QWidget* profilePicker();
    QWidget* platformNote(const QString& text);
    void refreshPage(int i);
    void onMetrics(const Metrics& m);
    void updateStatus();
    void editMacro(int index);  // -1 = neu

    App* app_;
    QStackedWidget* stack_ = nullptr;
    QList<QPushButton*> nav_;
    QLabel* status_ = nullptr;
    struct Tile {
        QLabel* value = nullptr;
        QLabel* sub = nullptr;
        Sparkline* spark = nullptr;
    };
    QHash<QString, Tile> tiles_;
    QPointer<OverlayPreview> preview_;
    QPointer<QLabel> fpsInfo_;
    int editProfile_ = 0;
    int page_ = 0;
};

// Makro bearbeiten: Name, Auslöser, Ablauf, Schritte als Text, Aufnahme (Windows)
class MacroDialog : public QDialog {
    Q_OBJECT
public:
    MacroDialog(App* app, const Macro& m, QWidget* parent = nullptr);
    Macro macro() const;

private:
    void validate();
    App* app_;
    Macro m_;
    QLineEdit* name_;
    KeyButton* trigger_;
    QButtonGroup* mode_;
    QSpinBox* repeat_;
    QPlainTextEdit* text_;
    QLabel* info_;
    QPushButton* rec_ = nullptr;
    QPushButton* ok_;
};
