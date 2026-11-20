#include "mainwindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QColorDialog>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QScreen>
#include <QScrollArea>
#include <QSlider>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>
#include "app.h"
#include "i18n.h"
#include "look.h"
#include "overlay.h"
#include "platform.h"
#include "widgets.h"

static QString itemName(Item i) {
    switch (i) {
    case Item::Fps: return L("Frame rate (FPS)");
    case Item::Cpu: return L("CPU load");
    case Item::CpuTemp: return L("CPU temperature");
    case Item::Gpu: return L("GPU load");
    case Item::GpuTemp: return L("GPU temperature");
    case Item::Ram: return L("RAM");
    case Item::Vram: return L("Video memory (VRAM)");
    case Item::Ping: return L("Ping");
    case Item::Clock: return L("Clock");
    case Item::Battery: return L("Battery");
    default: return QString();
    }
}

static QString fpsStateText(FpsState s) {
    switch (s) {
    case FpsState::Ok: return L("Frame rate is measured (DirectX, Vulkan and OpenGL through Windows event tracing).");
    case FpsState::NoFrames: return L("Frame rate is ready. It appears as soon as a game is in front.");
    case FpsState::Starting: return L("Frame rate is starting…");
    case FpsState::NeedsAdmin:
        return L("Windows only lets programs with admin rights count frames. Sysline doesn't guess, so the overlay says "
                 "“admin rights needed” until you either start Sysline as administrator or allow it once.");
    case FpsState::Unsupported:
    default:
#ifdef Q_OS_WIN
        return L("Frame rate isn't available on this Windows version.");
#else
        return L("The frame rate of other programs can only be read on Windows. Everything else works here too.");
#endif
    }
}

// ---------------------------------------------------------------- Fenster
MainWindow::MainWindow(App* app) : app_(app) {
    setObjectName("mainroot");
    setAttribute(Qt::WA_StyledBackground);
    setWindowTitle("Sysline");
    setWindowIcon(syslineIcon());
    resize(1080, 720);
    connect(app_, &App::metricsChanged, this, &MainWindow::onMetrics);
    connect(app_, &App::profileChanged, this, [this] { updateStatus(); });
    rebuild();
}

void MainWindow::closeEvent(QCloseEvent* e) {
    // Fenster zu = Sysline läuft im Tray weiter (Overlay und Tasten bleiben aktiv)
    e->accept();
}

void MainWindow::rebuild() {
    const int keep = page_;
    if (layout()) {
        QLayoutItem* it;
        while ((it = layout()->takeAt(0))) {
            if (it->widget()) {
                it->widget()->hide();
                it->widget()->deleteLater();
            }
            delete it;
        }
        delete layout();
    }
    nav_.clear();
    tiles_.clear();
    app_->applyLook();
    const Palette pal = makePalette(app_->config().accent, app_->config().dark);

    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);
    auto* side = new QFrame;
    side->setObjectName("side");
    side->setFixedWidth(220);
    auto* sv = new QVBoxLayout(side);
    sv->setContentsMargins(16, 22, 16, 18);
    sv->setSpacing(4);
    auto* brand = new QHBoxLayout;
    auto* logo = new QLabel;
    logo->setPixmap(syslineLogo(40));
    brand->addWidget(logo);
    auto* bt = new QVBoxLayout;
    bt->setSpacing(0);
    bt->addWidget(mkLabel("Sysline", "h2", false));
    bt->addWidget(mkLabel(QString("Version %1").arg(APP_VERSION), "tiny", false));
    brand->addLayout(bt, 1);
    sv->addLayout(brand);
    sv->addSpacing(18);
    const QList<QPair<QString, Ic>> pages = {{L("Overview"), Ic::Gauge},  {L("Overlay"), Ic::Overlay},
                                             {L("Remap keys"), Ic::Keys}, {L("Macros"), Ic::Macro},
                                             {L("Profiles"), Ic::Profile}, {L("Settings"), Ic::Gear}};
    for (int i = 0; i < pages.size(); ++i) {
        auto* b = new QPushButton(pages[i].first);
        b->setObjectName("nav");
        b->setCheckable(true);
        b->setIcon(icon(pages[i].second, pal.muted));
        b->setIconSize(QSize(18, 18));
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this, i] { showPage(i); });
        nav_ << b;
        sv->addWidget(b);
    }
    sv->addStretch();
    status_ = mkLabel(QString(), "tiny");
    sv->addWidget(status_);
    h->addWidget(side);

    stack_ = new QStackedWidget;
    stack_->addWidget(wrap(overviewPage()));
    stack_->addWidget(wrap(overlayPage()));
    stack_->addWidget(wrap(keysPage()));
    stack_->addWidget(wrap(macrosPage()));
    stack_->addWidget(wrap(profilesPage()));
    stack_->addWidget(wrap(settingsPage()));
    h->addWidget(stack_, 1);
    showPage(keep);
    updateStatus();
    onMetrics(app_->metrics());
}

QWidget* MainWindow::wrap(QWidget* content) {
    auto* area = new QScrollArea;
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area->setWidget(content);
    enableSmoothScroll(area);
    return area;
}

void MainWindow::showPage(int i) {
    page_ = std::clamp(i, 0, int(PageCount) - 1);
    if (stack_) stack_->setCurrentIndex(page_);
    for (int k = 0; k < nav_.size(); ++k) nav_[k]->setChecked(k == page_);
}

void MainWindow::refreshPage(int i) {
    QWidget* fresh = nullptr;
    switch (i) {
    case PageKeys: fresh = keysPage(); break;
    case PageMacros: fresh = macrosPage(); break;
    case PageProfiles: fresh = profilesPage(); break;
    case PageOverlay: fresh = overlayPage(); break;
    default: return;
    }
    QWidget* old = stack_->widget(i);
    stack_->insertWidget(i, wrap(fresh));
    stack_->removeWidget(old);
    old->deleteLater();
    showPage(page_);
}

void MainWindow::setEditProfile(int i) {
    editProfile_ = std::clamp(i, 0, int(app_->config().profiles.size()) - 1);
    refreshPage(PageKeys);
    refreshPage(PageMacros);
}

void MainWindow::updateStatus() {
    if (!status_) return;
    const Config& c = app_->config();
    const QString prog = QFileInfo(app_->activeProgram()).fileName();
    status_->setText(QString("%1: <b>%2</b><br>%3: %4")
                         .arg(L("Profile"), c.profiles.value(app_->activeProfile()).name.toHtmlEscaped(), L("In front"),
                              prog.isEmpty() ? QString("–") : prog.toHtmlEscaped()));
}

QWidget* MainWindow::platformNote(const QString& text) {
    QVBoxLayout* v;
    QFrame* card = mkCard(&v, "note");
    v->addWidget(mkLabel(text));
    return card;
}

static QWidget* pageBox(QVBoxLayout** out, const QString& title, const QString& sub) {
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(30, 26, 30, 26);
    v->setSpacing(12);
    v->addWidget(mkLabel(title, "h1"));
    if (!sub.isEmpty()) v->addWidget(mkLabel(sub, "muted"));
    v->addSpacing(4);
    *out = v;
    return w;
}

// ---------------------------------------------------------------- Übersicht
QWidget* MainWindow::overviewPage() {
    QVBoxLayout* v;
    QWidget* w = pageBox(&v, L("Overview"), L("Live values of this computer. The overlay shows the same over your game."));
    const Config& c = app_->config();
    auto* bar = new QHBoxLayout;
    auto* toggle = mkToggle(L("Show overlay"), c.overlay.visible);
    connect(toggle, &QCheckBox::toggled, this, [this](bool on) { app_->setOverlayVisible(on); });
    connect(app_, &App::settingsChanged, toggle, [this, toggle] {
        QSignalBlocker b(toggle);
        toggle->setChecked(app_->config().overlay.visible);
    });
    bar->addWidget(toggle);
    bar->addSpacing(10);
    const QString keyText = hotkeyText(c.overlay.hotkey);
    auto* key = mkLabel(keyText.isEmpty() ? QString("—") : keyText, "key", false);
    bar->addWidget(key);
    bar->addWidget(mkLabel(uiHotkeySupported() ? L("shows and hides it in any game")
                                                     : L("(global keys need X11 here – use the tray menu)"),
                           "muted", false));
    bar->addStretch();
    v->addLayout(bar);

    auto* grid = new QGridLayout;
    grid->setSpacing(12);
    const Palette pal = makePalette(c.accent, c.dark);
    const QList<QPair<QString, QString>> tiles = {{"fps", L("Frame rate")}, {"cpu", L("Processor")},  {"gpu", L("Graphics card")},
                                                  {"gputemp", L("GPU temperature")}, {"ram", L("Memory")},
                                                  {"vram", L("Video memory")}, {"ping", L("Ping")},
                                                  {"cputemp", L("CPU temperature")}, {"battery", L("Battery")}};
    for (int i = 0; i < tiles.size(); ++i) {
        QVBoxLayout* tv;
        QFrame* card = mkCard(&tv);
        tv->setSpacing(2);
        tv->addWidget(mkLabel(tiles[i].second.toUpper(), "tiny", false));
        Tile t;
        t.value = mkLabel("–", "value", false);
        t.sub = mkLabel(QString(), "unit");
        t.spark = new Sparkline;
        t.spark->setColor(pal.accent);
        if (tiles[i].first == "fps") t.spark->setRange(60);
        if (tiles[i].first == "ping") t.spark->setRange(50);
        tv->addWidget(t.value);
        tv->addWidget(t.sub);
        tv->addWidget(t.spark);
        tiles_.insert(tiles[i].first, t);
        grid->addWidget(card, i / 3, i % 3);
    }
    v->addLayout(grid);
    v->addStretch();
    return w;
}

void MainWindow::fillDemoHistory() {
    const QHash<QString, QPair<double, double>> demo = {{"fps", {141, 9}},  {"cpu", {34, 9}},     {"gpu", {93, 4}},
                                                       {"gputemp", {63, 2}}, {"ram", {58, 2}},    {"vram", {74, 3}},
                                                       {"ping", {15, 3}},   {"cputemp", {61, 3}}, {"battery", {80, 0.5}}};
    int seed = 1;
    for (auto it = tiles_.begin(); it != tiles_.end(); ++it, ++seed)
        if (demo.contains(it.key())) it->spark->fillDemo(demo[it.key()].first, demo[it.key()].second, seed);
}

void MainWindow::onMetrics(const Metrics& m) {
    if (preview_) preview_->setMetrics(m);
    if (fpsInfo_) fpsInfo_->setText(fpsStateText(m.fpsState));
    updateStatus();
    if (tiles_.isEmpty()) return;
    auto set = [&](const char* key, const QString& value, const QString& sub, double spark) {
        auto it = tiles_.find(key);
        if (it == tiles_.end()) return;
        it->value->setText(value);
        it->sub->setText(sub);
        it->spark->add(spark);
    };
    const QString dash = "–";
    QString fpsSub;
    switch (m.fpsState) {
    case FpsState::Ok: fpsSub = QFileInfo(m.foreground).fileName(); break;
    case FpsState::NeedsAdmin: fpsSub = L("admin rights needed – see Overlay"); break;
    case FpsState::Unsupported: fpsSub = L("Windows only"); break;
    default: fpsSub = L("no game in front"); break;
    }
    set("fps", m.fpsState == FpsState::Ok ? QString::number(int(std::lround(m.fps))) : dash, fpsSub, m.fps);
    set("cpu", m.cpu >= 0 ? QString("%1 %").arg(int(std::lround(m.cpu))) : dash, m.cpuName, m.cpu);
    set("gpu", m.gpu >= 0 ? QString("%1 %").arg(int(std::lround(m.gpu))) : dash, m.gpuName, m.gpu);
    set("gputemp", m.gpuTemp > 0 ? QString("%1 °C").arg(int(std::lround(m.gpuTemp))) : dash,
        m.gpuHotspot > 0 ? L("Hotspot %1 °C").arg(int(std::lround(m.gpuHotspot)))
                         : (m.gpuTemp > 0 ? QString() : L("no sensor found (AMD/NVIDIA driver needed)")),
        m.gpuTemp);
    set("ram", m.ramTotal > 0 ? QString("%1 GB").arg(fmtNumber(m.ramUsed, 1)) : dash,
        m.ramTotal > 0 ? L("of %1 GB").arg(fmtNumber(m.ramTotal, 0)) : QString(),
        m.ramTotal > 0 ? 100 * m.ramUsed / m.ramTotal : -1);
    set("vram", m.vramTotal > 0 ? QString("%1 GB").arg(fmtNumber(m.vramUsed, 1)) : dash,
        m.vramTotal > 0 ? L("of %1 GB").arg(fmtNumber(m.vramTotal, 0)) : L("not available"),
        m.vramTotal > 0 ? 100 * m.vramUsed / m.vramTotal : -1);
    set("ping", m.ping >= 0 ? QString("%1 ms").arg(m.ping < 1 ? QString("<1") : QString::number(int(std::lround(m.ping)))) : dash,
        app_->config().pingHost + (m.pingTcp ? " · TCP" : "") + (m.pingFailed ? " · " + L("no answer") : QString()), m.ping);
    set("cputemp", m.cpuTemp > 0 ? QString("%1 °C").arg(int(std::lround(m.cpuTemp))) : dash,
        m.cpuTemp > 0 ? QString() : L("not shown – see Overlay"), m.cpuTemp);
    if (auto it = tiles_.find("battery"); it != tiles_.end()) it->value->parentWidget()->setVisible(m.battery >= 0);
    set("battery", m.battery >= 0 ? QString("%1 %").arg(m.battery) : dash,
        m.battery >= 0 ? (m.charging ? L("charging") : L("on battery")) : L("no battery"), m.battery);
}

// ---------------------------------------------------------------- Overlay
QWidget* MainWindow::overlayPage() {
    QVBoxLayout* v;
    QWidget* w = pageBox(&v, L("Overlay"), L("Always on top, click-through and see-through. Works over games in borderless or windowed mode."));
    Config& c = app_->config();
    auto* cols = new QHBoxLayout;
    cols->setSpacing(22);
    auto* form = new QFormLayout;
    form->setSpacing(12);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto update = [this] {
        app_->changed();
        if (preview_) preview_->setOptions(app_->config().overlay, QColor(app_->config().accent));
    };
    auto* show = mkToggle(L("Show overlay"), c.overlay.visible);
    connect(show, &QCheckBox::toggled, this, [this, update](bool on) { app_->config().overlay.visible = on; update(); });
    connect(app_, &App::settingsChanged, show, [this, show] {
        QSignalBlocker b(show);
        show->setChecked(app_->config().overlay.visible);
    });
    form->addRow(show);
    auto* hk = new HotkeyButton(c.overlay.hotkey);
    auto* hkWarn = mkLabel(QString(), "tiny");
    auto checkKey = [this, hkWarn] {
        hkWarn->setText(app_->hotkeyOk() || app_->config().overlay.hotkey.isEmpty() ? QString()
                                                                                     : L("This key is taken by another program or not supported here."));
        hkWarn->setVisible(!hkWarn->text().isEmpty());
    };
    connect(hk, &HotkeyButton::sequenceChanged, this, [this, update, checkKey](const QString& s) {
        app_->config().overlay.hotkey = s;
        update();
        checkKey();
    });
    auto* hkBox = new QVBoxLayout;
    hkBox->addWidget(hk);
    hkBox->addWidget(hkWarn);
    form->addRow(L("Show / hide key"), hkBox);
    QButtonGroup* cg = nullptr;
    form->addRow(L("Corner"), mkSegmented({L("Top left"), L("Top right"), L("Bottom left"), L("Bottom right")}, int(c.overlay.corner), &cg));
    connect(cg, &QButtonGroup::idClicked, this, [this, update](int id) { app_->config().overlay.corner = Corner(id); update(); });
    QButtonGroup* lg = nullptr;
    form->addRow(L("Layout"), mkSegmented({L("List"), L("One line")}, c.overlay.horizontal ? 1 : 0, &lg));
    connect(lg, &QButtonGroup::idClicked, this, [this, update](int id) { app_->config().overlay.horizontal = id == 1; update(); });
    auto slider = [&](int min, int max, int val, const QString& suffix, std::function<void(int)> set) {
        auto* box = new QWidget;
        auto* bh = new QHBoxLayout(box);
        bh->setContentsMargins(0, 0, 0, 0);
        auto* s = new QSlider(Qt::Horizontal);
        s->setRange(min, max);
        s->setValue(val);
        auto* lbl = mkLabel(QString("%1%2").arg(val).arg(suffix), nullptr, false);
        lbl->setMinimumWidth(48);
        connect(s, &QSlider::valueChanged, this, [lbl, suffix, set](int x) {
            lbl->setText(QString("%1%2").arg(x).arg(suffix));
            set(x);
        });
        bh->addWidget(s, 1);
        bh->addWidget(lbl);
        return box;
    };
    form->addRow(L("Size"), slider(70, 200, c.overlay.scale, " %", [this, update](int x) { app_->config().overlay.scale = x; update(); }));
    form->addRow(L("Background"), slider(0, 100, c.overlay.opacity, " %", [this, update](int x) { app_->config().overlay.opacity = x; update(); }));
    auto* screen = new QComboBox;
    screen->addItem(L("Main screen"), QString());
    for (QScreen* s : QGuiApplication::screens())
        screen->addItem(QString("%1 · %2×%3").arg(s->name()).arg(s->size().width()).arg(s->size().height()), s->name());
    screen->setCurrentIndex(std::max(0, screen->findData(c.overlay.screen)));
    connect(screen, &QComboBox::currentIndexChanged, this, [this, screen, update] {
        app_->config().overlay.screen = screen->currentData().toString();
        update();
    });
    form->addRow(L("Screen"), screen);
    auto* ping = new QLineEdit(c.pingHost);
    ping->setPlaceholderText("mert.win");
    connect(ping, &QLineEdit::editingFinished, this, [this, ping, update] {
        app_->config().pingHost = ping->text().trimmed();
        update();
    });
    form->addRow(L("Ping to"), ping);
    QButtonGroup* ig = nullptr;
    const int ms = c.intervalMs;
    form->addRow(L("Refresh"), mkSegmented({"0,5 s", "1 s", "2 s"}, ms <= 500 ? 0 : ms <= 1000 ? 1 : 2, &ig));
    connect(ig, &QButtonGroup::idClicked, this, [this, update](int id) { app_->config().intervalMs = id == 0 ? 500 : id == 1 ? 1000 : 2000; update(); });
#ifdef Q_OS_WIN
    auto* cap = mkToggle(L("Hide from screenshots and streams"), c.overlay.hideFromCapture);
    connect(cap, &QCheckBox::toggled, this, [this, update](bool on) { app_->config().overlay.hideFromCapture = on; update(); });
    form->addRow(cap);
#endif
    auto* left = new QVBoxLayout;
    left->addLayout(form);
    left->addStretch();
    auto* valuesBox = new QVBoxLayout;
    valuesBox->addWidget(mkLabel(L("Values"), "h2"));
    auto* items = new QGridLayout;
    items->setHorizontalSpacing(18);
    for (int i = 0; i < kItemCount; ++i) {
        auto* t = mkToggle(itemName(Item(i)), c.overlay.items[i]);
        connect(t, &QCheckBox::toggled, this, [this, i, update](bool on) { app_->config().overlay.items[i] = on; update(); });
        items->addWidget(t, i, 0);
    }
    valuesBox->addLayout(items);
    valuesBox->addStretch();
    auto* top = new QHBoxLayout;
    top->setSpacing(18);
    preview_ = new OverlayPreview;
    preview_->setOptions(c.overlay, QColor(c.accent));
    preview_->setMetrics(app_->metrics());
    preview_->setMinimumHeight(300);
    top->addWidget(preview_, 3);
    auto* right = new QVBoxLayout;
    // Bildrate: ehrlich sagen, was geht
    QVBoxLayout* fv;
    QFrame* fcard = mkCard(&fv, "note");
    fv->addWidget(mkLabel(L("Frame rate"), "h2"));
    fpsInfo_ = mkLabel(fpsStateText(app_->metrics().fpsState));
    fv->addWidget(fpsInfo_);
#ifdef Q_OS_WIN
    if (!isElevated()) {
        auto* row = new QHBoxLayout;
        auto* admin = new QPushButton(L("Restart as administrator"));
        connect(admin, &QPushButton::clicked, this, [this] {
            if (restartElevated()) app_->quit();
        });
        auto* allow = new QPushButton(L("Allow without admin rights"));
        allow->setToolTip(L("Adds you to the Windows group “Performance Log Users” (asks for admin rights once). Takes effect after signing out and in again."));
        connect(allow, &QPushButton::clicked, this, [this] {
            const bool ok = allowFpsWithoutAdmin();
            QMessageBox::information(this, "Sysline",
                                     ok ? L("Done. Sign out of Windows and in again – then Sysline shows the frame rate without admin rights.")
                                        : L("That didn't work (cancelled or not allowed). You can still start Sysline as administrator."));
        });
        row->addWidget(admin);
        row->addWidget(allow);
        row->addStretch();
        fv->addLayout(row);
    }
#endif
    right->addWidget(fcard);
    QVBoxLayout* tv;
    QFrame* tcard = mkCard(&tv, "note");
    tv->addWidget(mkLabel(L("Temperatures"), "h2"));
    tv->addWidget(mkLabel(L("GPU temperature comes straight from the graphics driver: AMD through ADL (Adrenalin), NVIDIA through NVML. ") +
                          (g_demoUi ? L("Windows only reports the processor temperature to programs with their own kernel driver. Sysline doesn't install one, so this value isn't shown on Windows.")
                                    : Sampler::cpuTempNote())));
    right->addWidget(tcard);
    right->addWidget(mkLabel(L("Exclusive fullscreen games draw over everything – switch the game to borderless window to see the overlay."), "tiny"));
    right->addStretch();
    top->addLayout(right, 2);
    v->addLayout(top);
    v->addSpacing(8);
    cols->addLayout(left, 3);
    cols->addLayout(valuesBox, 2);
    v->addLayout(cols);
    checkKey();
    return w;
}

// ---------------------------------------------------------------- Profilwahl oben auf Tasten/Makros
QWidget* MainWindow::profilePicker() {
    auto* box = new QWidget;
    auto* h = new QHBoxLayout(box);
    h->setContentsMargins(0, 0, 0, 0);
    h->addWidget(mkLabel(L("Profile"), nullptr, false));
    auto* combo = new QComboBox;
    for (const Profile& p : app_->config().profiles) combo->addItem(p.name);
    combo->setCurrentIndex(editProfile_);
    combo->setMinimumWidth(200);
    connect(combo, &QComboBox::activated, this, [this](int i) { QTimer::singleShot(0, this, [this, i] { setEditProfile(i); }); });
    h->addWidget(combo);
    const Profile& p = app_->config().profiles[editProfile_];
    h->addWidget(mkLabel(editProfile_ == 0 ? L("applies to every program without its own profile")
                                           : L("for %1").arg(p.programs.isEmpty() ? L("(no program yet)") : p.programs.join(", ")),
                         "muted", false));
    h->addStretch();
    return box;
}

// ---------------------------------------------------------------- Tasten umbelegen
QWidget* MainWindow::keysPage() {
    QVBoxLayout* v;
    QWidget* w = pageBox(&v, L("Remap keys"), L("Press one key, get another – or a whole combination. Each profile has its own list."));
    if (!uiKeysSupported())
        v->addWidget(platformNote(L("Remapping keys only works on Windows. On this system you can prepare profiles here, "
                                    "they work as soon as you use them on Windows.")));
    Config& c = app_->config();
    editProfile_ = std::clamp(editProfile_, 0, int(c.profiles.size()) - 1);
    v->addWidget(profilePicker());
    auto* top = new QHBoxLayout;
    auto* global = mkToggle(L("Remapping on (all profiles)"), c.remapAllowed);
    connect(global, &QCheckBox::toggled, this, [this](bool on) { app_->config().remapAllowed = on; app_->changed(); });
    auto* here = mkToggle(L("Remapping in this profile"), c.profiles[editProfile_].remapOn);
    connect(here, &QCheckBox::toggled, this, [this](bool on) { app_->config().profiles[editProfile_].remapOn = on; app_->changed(); });
    top->addWidget(global);
    top->addSpacing(24);
    top->addWidget(here);
    top->addStretch();
    v->addLayout(top);
    const Palette pal = makePalette(c.accent, c.dark);
    QList<Remap>& list = c.profiles[editProfile_].remaps;
    if (list.isEmpty()) v->addWidget(mkLabel(L("No keys remapped yet."), "muted"));
    for (int i = 0; i < list.size(); ++i) {
        QVBoxLayout* cv;
        QFrame* card = mkCard(&cv);
        auto* row = new QHBoxLayout;
        cv->addLayout(row);
        auto* from = new KeyButton(KeyCombo{{}, list[i].from}, false);
        auto* arrow = new QLabel;
        arrow->setPixmap(icon(Ic::Arrow, pal.muted).pixmap(22, 22));
        auto* to = new KeyButton(list[i].to, true);
        auto* on = mkToggle(QString(), list[i].enabled);
        on->setToolTip(L("On / off"));
        auto* del = new QPushButton;
        del->setObjectName("flat");
        del->setIcon(icon(Ic::Trash, pal.muted));
        del->setToolTip(L("Remove"));
        row->addWidget(from);
        row->addWidget(arrow);
        row->addWidget(to);
        row->addStretch();
        row->addWidget(on);
        row->addWidget(del);
        connect(from, &KeyButton::comboChanged, this, [this, i](const KeyCombo& k) {
            app_->config().profiles[editProfile_].remaps[i].from = k.key;
            app_->changed();
        });
        connect(to, &KeyButton::comboChanged, this, [this, i](const KeyCombo& k) {
            app_->config().profiles[editProfile_].remaps[i].to = k;
            app_->changed();
        });
        connect(on, &QCheckBox::toggled, this, [this, i](bool x) {
            app_->config().profiles[editProfile_].remaps[i].enabled = x;
            app_->changed();
        });
        connect(del, &QPushButton::clicked, this, [this, i] {
            app_->config().profiles[editProfile_].remaps.removeAt(i);
            app_->changed();
            QTimer::singleShot(0, this, [this] { refreshPage(PageKeys); });
        });
        v->addWidget(card);
    }
    auto* add = new QPushButton(L("+  Remap a key"));
    add->setObjectName("dashed");
    add->setCursor(Qt::PointingHandCursor);
    connect(add, &QPushButton::clicked, this, [this] {
        Remap r;
        r.from = vk::CapsLock;
        r.to.key = vk::Ctrl;
        app_->config().profiles[editProfile_].remaps << r;
        app_->changed();
        QTimer::singleShot(0, this, [this] { refreshPage(PageKeys); });
    });
    v->addWidget(add);
    v->addWidget(mkLabel(L("Tip: click a key field and press the key. Right-click for a list – there you can also switch a key off "
                           "(e.g. the Windows key while gaming). Games running as administrator only see remapped keys when Sysline "
                           "runs as administrator too."),
                         "tiny"));
    v->addStretch();
    return w;
}

// ---------------------------------------------------------------- Makros
QWidget* MainWindow::macrosPage() {
    QVBoxLayout* v;
    QWidget* w = pageBox(&v, L("Macros"), L("A key plays a sequence of keys with pauses – recorded or typed."));
    Config& c = app_->config();
    QVBoxLayout* wv;
    QFrame* warn = mkCard(&wv, "warn");
    wv->addWidget(mkLabel(L("Careful in online games"), "h2"));
    wv->addWidget(mkLabel(L("Many online games forbid macros in their rules, and anti-cheat software (e.g. in competitive shooters) "
                            "can detect them and ban your account. Only use macros where the game allows them. "
                            "Macros are therefore off until you switch them on here.")));
    auto* allowed = mkToggle(L("Allow macros"), c.macrosAllowed);
    connect(allowed, &QCheckBox::toggled, this, [this, allowed](bool on) {
        if (on && !app_->config().macrosAllowed) {
            const auto r = QMessageBox::warning(this, "Sysline",
                                                L("Switch macros on?\n\nSome games ban accounts for macros. You are responsible for "
                                                  "using them only where the rules allow it."),
                                                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (r != QMessageBox::Yes) {
                QSignalBlocker b(allowed);
                allowed->setChecked(false);
                return;
            }
        }
        app_->config().macrosAllowed = on;
        app_->changed();
    });
    wv->addWidget(allowed);
    v->addWidget(warn);
    if (!uiKeysSupported())
        v->addWidget(platformNote(L("Macros only play on Windows. On this system you can write and save them.")));
    editProfile_ = std::clamp(editProfile_, 0, int(c.profiles.size()) - 1);
    v->addWidget(profilePicker());
    auto* here = mkToggle(L("Macros in this profile"), c.profiles[editProfile_].macrosOn);
    connect(here, &QCheckBox::toggled, this, [this](bool on) { app_->config().profiles[editProfile_].macrosOn = on; app_->changed(); });
    v->addWidget(here);
    const Palette pal = makePalette(c.accent, c.dark);
    const QList<Macro>& list = c.profiles[editProfile_].macros;
    if (list.isEmpty()) v->addWidget(mkLabel(L("No macros in this profile yet."), "muted"));
    for (int i = 0; i < list.size(); ++i) {
        QVBoxLayout* cv;
        QFrame* card = mkCard(&cv);
        auto* row = new QHBoxLayout;
        cv->addLayout(row);
        auto* key = mkLabel(list[i].trigger ? keyLabel(list[i].trigger) : QString("—"), "key", false);
        auto* text = new QVBoxLayout;
        text->setSpacing(0);
        text->addWidget(mkLabel(list[i].name.isEmpty() ? L("Macro %1").arg(i + 1) : list[i].name, "h2", false));
        text->addWidget(mkLabel(macroSummary(list[i]), "muted", false));
        auto* on = mkToggle(QString(), list[i].enabled);
        auto* edit = new QPushButton;
        edit->setObjectName("flat");
        edit->setIcon(icon(Ic::Edit, pal.muted));
        edit->setToolTip(L("Edit"));
        auto* del = new QPushButton;
        del->setObjectName("flat");
        del->setIcon(icon(Ic::Trash, pal.muted));
        del->setToolTip(L("Remove"));
        row->addWidget(key);
        row->addSpacing(8);
        row->addLayout(text, 1);
        row->addWidget(on);
        row->addWidget(edit);
        row->addWidget(del);
        connect(on, &QCheckBox::toggled, this, [this, i](bool x) {
            app_->config().profiles[editProfile_].macros[i].enabled = x;
            app_->changed();
        });
        connect(edit, &QPushButton::clicked, this, [this, i] { editMacro(i); });
        connect(del, &QPushButton::clicked, this, [this, i] {
            app_->config().profiles[editProfile_].macros.removeAt(i);
            app_->changed();
            QTimer::singleShot(0, this, [this] { refreshPage(PageMacros); });
        });
        v->addWidget(card);
    }
    auto* add = new QPushButton(L("+  New macro"));
    add->setObjectName("dashed");
    add->setCursor(Qt::PointingHandCursor);
    connect(add, &QPushButton::clicked, this, [this] { editMacro(-1); });
    v->addWidget(add);
    v->addStretch();
    return w;
}

void MainWindow::editMacro(int index) {
    QList<Macro>& list = app_->config().profiles[editProfile_].macros;
    Macro m;
    if (index >= 0 && index < list.size()) m = list[index];
    else {
        m.name = L("Macro %1").arg(list.size() + 1);
        m.trigger = vk::F8;
        m.steps = {MacroStep{MacroStep::Tap, vk::KeyA + ('Q' - 'A'), 0}, MacroStep{MacroStep::Wait, 0, 50},
                   MacroStep{MacroStep::Tap, vk::KeyA + ('E' - 'A'), 0}};
    }
    MacroDialog dlg(app_, m, this);
    if (dlg.exec() != QDialog::Accepted) return;
    QList<Macro>& now = app_->config().profiles[editProfile_].macros;
    if (index >= 0 && index < now.size()) now[index] = dlg.macro();
    else now << dlg.macro();
    app_->changed();
    refreshPage(PageMacros);
}

MacroDialog::MacroDialog(App* app, const Macro& m, QWidget* parent) : QDialog(parent), app_(app), m_(m) {
    setWindowTitle(L("Edit macro") + " – Sysline");
    resize(560, 620);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(22, 20, 22, 18);
    v->setSpacing(10);
    auto* form = new QFormLayout;
    form->setSpacing(10);
    name_ = new QLineEdit(m.name);
    form->addRow(L("Name"), name_);
    trigger_ = new KeyButton(KeyCombo{{}, m.trigger}, false);
    form->addRow(L("Starts with"), trigger_);
    auto* modeBox = new QWidget;
    auto* mh = new QHBoxLayout(modeBox);
    mh->setContentsMargins(0, 0, 0, 0);
    mh->addWidget(mkSegmented({L("Once"), L("Repeat"), L("While held")}, int(m.mode), &mode_), 1);
    repeat_ = new QSpinBox;
    repeat_->setRange(1, 1000);
    repeat_->setValue(m.repeat);
    repeat_->setSuffix("×");
    mh->addWidget(repeat_);
    form->addRow(L("Plays"), modeBox);
    v->addLayout(form);
    v->addWidget(mkLabel(L("Steps – one per line: “tap A”, “down Shift”, “up Shift”, “wait 50” (milliseconds)."), "muted"));
    text_ = new QPlainTextEdit(formatSteps(m.steps, language() == "de"));
    v->addWidget(text_, 1);
    info_ = mkLabel(QString(), "tiny");
    v->addWidget(info_);
    auto* row = new QHBoxLayout;
    if (uiKeysSupported()) {
        rec_ = new QPushButton(L("Record"));
        rec_->setIcon(icon(Ic::Record, makePalette(app->config().accent, app->config().dark).accent));
        connect(rec_, &QPushButton::clicked, this, [this] {
            if (!app_->recording()) {
                app_->startRecording();
                rec_->setText(L("Stop recording"));
                info_->setText(L("Recording… press the keys (also in the game), then come back and stop."));
                return;
            }
            const QList<MacroStep> steps = stepsFromRecording(app_->stopRecording());
            rec_->setText(L("Record"));
            text_->setPlainText(formatSteps(steps, language() == "de"));
        });
        row->addWidget(rec_);
    }
    row->addStretch();
    auto* cancel = new QPushButton(L("Cancel"));
    ok_ = new QPushButton(L("Save"));
    ok_->setObjectName("primary");
    ok_->setDefault(true);
    row->addWidget(cancel);
    row->addWidget(ok_);
    v->addLayout(row);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok_, &QPushButton::clicked, this, [this] {
        if (app_->recording()) app_->stopRecording();
        accept();
    });
    connect(this, &QDialog::rejected, this, [this] { if (app_->recording()) app_->stopRecording(); });
    connect(text_, &QPlainTextEdit::textChanged, this, &MacroDialog::validate);
    connect(mode_, &QButtonGroup::idClicked, this, &MacroDialog::validate);
    connect(repeat_, &QSpinBox::valueChanged, this, &MacroDialog::validate);
    validate();
}

void MacroDialog::validate() {
    int bad = 0;
    const QList<MacroStep> steps = parseSteps(text_->toPlainText(), &bad);
    repeat_->setVisible(mode_->checkedId() == Macro::Repeat);
    if (app_->recording()) return;
    if (bad) info_->setText(L("Line %1 isn't understood.").arg(bad));
    else {
        Macro m = macro();
        info_->setText(macroSummary(m));
    }
    ok_->setEnabled(!bad && !steps.isEmpty());
}

Macro MacroDialog::macro() const {
    Macro m = m_;
    m.name = name_->text().trimmed();
    m.trigger = trigger_->combo().key;
    m.mode = Macro::Mode(std::max(0, mode_->checkedId()));
    m.repeat = repeat_->value();
    m.steps = parseSteps(text_->toPlainText());
    return m;
}

// ---------------------------------------------------------------- Profile
QWidget* MainWindow::profilesPage() {
    QVBoxLayout* v;
    QWidget* w = pageBox(&v, L("Profiles"), L("Each game can get its own keys and macros. Sysline sees which program is in front and switches by itself."));
    Config& c = app_->config();
    auto* autoT = mkToggle(L("Switch profile automatically"), c.autoProfile);
    connect(autoT, &QCheckBox::toggled, this, [this](bool on) { app_->config().autoProfile = on; app_->changed(); updateStatus(); });
    v->addWidget(autoT);
#ifdef Q_OS_LINUX
    if (!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        v->addWidget(platformNote(L("Under Wayland the program in front can't be read, so automatic switching only sees X11 (XWayland) programs.")));
#endif
    const Palette pal = makePalette(c.accent, c.dark);
    for (int i = 0; i < c.profiles.size(); ++i) {
        QVBoxLayout* cv;
        QFrame* card = mkCard(&cv);
        auto* head = new QHBoxLayout;
        auto* name = new QLineEdit(c.profiles[i].name);
        name->setMaximumWidth(260);
        connect(name, &QLineEdit::editingFinished, this, [this, i, name] {
            const QString n = name->text().trimmed();
            if (n.isEmpty()) return;
            app_->config().profiles[i].name = n;
            app_->changed();
        });
        head->addWidget(name);
        if (i == app_->activeProfile()) head->addWidget(mkLabel(L("active now"), "pill", false));
        head->addStretch();
        head->addWidget(mkLabel(L("%1 keys · %2 macros").arg(c.profiles[i].remaps.size()).arg(c.profiles[i].macros.size()), "muted", false));
        auto* keys = new QPushButton(L("Keys"));
        connect(keys, &QPushButton::clicked, this, [this, i] { setEditProfile(i); showPage(PageKeys); });
        head->addWidget(keys);
        if (i > 0) {
            auto* del = new QPushButton;
            del->setObjectName("flat");
            del->setIcon(icon(Ic::Trash, pal.muted));
            del->setToolTip(L("Remove"));
            connect(del, &QPushButton::clicked, this, [this, i] {
                if (QMessageBox::question(this, "Sysline", L("Remove the profile “%1”?").arg(app_->config().profiles[i].name)) != QMessageBox::Yes)
                    return;
                app_->config().profiles.removeAt(i);
                editProfile_ = 0;
                app_->changed();
                QTimer::singleShot(0, this, [this] { refreshPage(PageProfiles); refreshPage(PageKeys); refreshPage(PageMacros); });
            });
            head->addWidget(del);
        }
        cv->addLayout(head);
        if (i == 0) {
            cv->addWidget(mkLabel(L("Standard profile: applies to every program that has no profile of its own."), "muted"));
        } else {
            auto* prow = new QHBoxLayout;
            auto* progs = new QLineEdit(c.profiles[i].programs.join(", "));
            progs->setPlaceholderText(L("e.g. game.exe, *-Win64-Shipping.exe"));
            connect(progs, &QLineEdit::editingFinished, this, [this, i, progs] {
                QStringList l;
                for (const QString& s : progs->text().split(',', Qt::SkipEmptyParts))
                    if (!s.trimmed().isEmpty()) l << s.trimmed();
                app_->config().profiles[i].programs = l;
                app_->changed();
            });
            auto* pick = new QPushButton(L("Add program"));
            connect(pick, &QPushButton::clicked, this, [this, i, pick, progs] {
                QMenu menu;
                const QStringList recent = app_->recentPrograms();
                if (recent.isEmpty()) menu.addAction(L("Start the game once – it then shows up here"))->setEnabled(false);
                for (const QString& r : recent)
                    menu.addAction(r, this, [this, i, r, progs] {
                        QStringList& l = app_->config().profiles[i].programs;
                        if (!l.contains(r, Qt::CaseInsensitive)) l << r;
                        progs->setText(l.join(", "));
                        app_->changed();
                    });
                menu.exec(pick->mapToGlobal(QPoint(0, pick->height())));
            });
            prow->addWidget(mkLabel(L("Programs"), nullptr, false));
            prow->addWidget(progs, 1);
            prow->addWidget(pick);
            cv->addLayout(prow);
        }
        v->addWidget(card);
    }
    auto* add = new QPushButton(L("+  New profile"));
    add->setObjectName("dashed");
    add->setCursor(Qt::PointingHandCursor);
    connect(add, &QPushButton::clicked, this, [this] {
        Profile p;
        p.name = L("Profile %1").arg(app_->config().profiles.size() + 1);
        const QStringList recent = app_->recentPrograms();
        if (!recent.isEmpty()) {
            p.programs << recent.first();
            p.name = QFileInfo(recent.first()).completeBaseName();
        }
        app_->config().profiles << p;
        app_->changed();
        QTimer::singleShot(0, this, [this] { refreshPage(PageProfiles); refreshPage(PageKeys); refreshPage(PageMacros); });
    });
    v->addWidget(add);
    v->addStretch();
    return w;
}

// ---------------------------------------------------------------- Einstellungen
QWidget* MainWindow::settingsPage() {
    QVBoxLayout* v;
    QWidget* w = pageBox(&v, L("Settings"), QString());
    Config& c = app_->config();
    auto* form = new QFormLayout;
    form->setSpacing(14);
    QButtonGroup* lg = nullptr;
    form->addRow(L("Language"), mkSegmented({"Deutsch", "English"}, language() == "de" ? 0 : 1, &lg));
    connect(lg, &QButtonGroup::idClicked, this, [this](int id) {
        app_->config().language = id == 0 ? "de" : "en";
        app_->changed();
        QTimer::singleShot(0, this, &MainWindow::rebuild);
    });
    auto* autostart = mkToggle(L("Start with the computer (in the tray)"), c.autostart);
    connect(autostart, &QCheckBox::toggled, this, [this](bool on) { app_->config().autostart = on; app_->changed(); });
    form->addRow(L("Start"), autostart);

    // Akzentfarbe wie in den anderen Line-Programmen
    auto* sw = new QWidget;
    auto* sh = new QHBoxLayout(sw);
    sh->setContentsMargins(0, 0, 0, 0);
    sh->setSpacing(8);
    auto* group = new QButtonGroup(sw);
    bool known = false;
    for (const QColor& col : accentChoices()) {
        auto* b = new QPushButton;
        b->setObjectName("swatch");
        b->setCheckable(true);
        b->setStyleSheet(QString("background:%1;").arg(col.name()));
        const bool on = col.name().compare(c.accent, Qt::CaseInsensitive) == 0;
        known |= on;
        b->setChecked(on);
        b->setCursor(Qt::PointingHandCursor);
        group->addButton(b);
        sh->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, col] {
            app_->config().accent = col.name();
            app_->changed();
            QTimer::singleShot(0, this, &MainWindow::rebuild);
        });
    }
    auto* own = new QPushButton(L("Any colour…"));
    connect(own, &QPushButton::clicked, this, [this] {
        const QColor col = QColorDialog::getColor(QColor(app_->config().accent), this, L("Accent colour"));
        if (!col.isValid()) return;
        app_->config().accent = col.name();
        app_->changed();
        QTimer::singleShot(0, this, &MainWindow::rebuild);
    });
    sh->addWidget(own);
    sh->addStretch();
    form->addRow(L("Accent colour"), sw);
    QButtonGroup* mg = nullptr;
    form->addRow(L("Mode"), mkSegmented({L("Dark"), L("Light")}, c.dark ? 0 : 1, &mg));
    connect(mg, &QButtonGroup::idClicked, this, [this](int id) {
        app_->config().dark = id == 0;
        app_->changed();
        QTimer::singleShot(0, this, &MainWindow::rebuild);
    });
    QButtonGroup* stg = nullptr;
    form->addRow(L("Style"), mkSegmented({L("Standard"), L("Minimal"), L("Acrylic")}, c.acryl ? 2 : c.minimal ? 1 : 0, &stg));
    connect(stg, &QButtonGroup::idClicked, this, [this](int id) {
        app_->config().minimal = id == 1;
        app_->config().acryl = id == 2;
        app_->changed();
        QTimer::singleShot(0, this, &MainWindow::rebuild);
    });
    v->addLayout(form);
    v->addSpacing(10);

    QVBoxLayout* av;
    QFrame* about = mkCard(&av);
    av->addWidget(mkLabel(QString("Sysline %1").arg(APP_VERSION), "h2"));
    av->addWidget(mkLabel(L("Game overlay, key remapping and macros in one program. Part of the W Software “Line” series. Free and open source (Apache 2.0)."), "muted"));
    auto* row = new QHBoxLayout;
    auto* upd = new QPushButton(L("Check for updates"));
    connect(upd, &QPushButton::clicked, this, [this] { app_->checkUpdates(); });
    auto* site = new QPushButton(L("Website"));
    connect(site, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl("https://mert.win/sys/")); });
    auto* log = new QPushButton(L("What's new"));
    connect(log, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl("https://mert.win/sys/changelog.html")); });
    row->addWidget(upd);
    row->addWidget(site);
    row->addWidget(log);
    row->addStretch();
    av->addLayout(row);
    v->addWidget(about);
    v->addWidget(mkLabel(L("Closing the window keeps Sysline running in the tray, so the overlay and your keys stay active. Quit from the tray menu."), "tiny"));
    v->addStretch();
    return w;
}
