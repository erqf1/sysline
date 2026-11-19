#include <QtGlobal>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl.h>
#endif
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLocalSocket>
#include <QPainter>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QTime>
#include <QTimer>
#include <cmath>
#include "app.h"
#include "config.h"
#include "engine.h"
#include "i18n.h"
#include "look.h"
#include "mainwindow.h"
#include "overlay.h"
#include "platform.h"
#include "sensors.h"
#include "widgets.h"

static bool sendToRunning(const QByteArray& cmd) {
    QLocalSocket s;
    s.connectToServer(serverName());
    if (!s.waitForConnected(1200)) return false;
    s.write(cmd);
    s.waitForBytesWritten(1200);
    s.waitForDisconnected(1200);
    return true;
}

// ---------------------------------------------------------------- Beispieldaten (Selbsttest und Website-Bilder)
static Metrics demoMetrics() {
    Metrics m;
    m.fps = 144;
    m.fpsState = FpsState::Ok;
    m.cpu = 34;
    m.cpuTemp = 61;
    m.cpuName = "AMD Ryzen 5 5600X 6-Core Processor";
    m.gpu = 96;
    m.gpuTemp = 64;
    m.gpuHotspot = 79;
    m.gpuName = "AMD Radeon RX 6600";
    m.ramUsed = 9.8;
    m.ramTotal = 16;
    m.vramUsed = 6.1;
    m.vramTotal = 8;
    m.ping = 14;
    m.battery = -1;
    m.foreground = "C:/Games/Skyforge Arena/arena.exe";
    return m;
}

static Config demoConfig(const QString& lang) {
    Config c;
    c.language = lang;
    c.firstRunDone = true;
    c.macrosAllowed = true;
    c.ensureDefaultProfile();
    c.profiles[0].remaps << Remap{vk::CapsLock, KeyCombo{{}, vk::Ctrl}, true} << Remap{vk::LWin, KeyCombo{}, true};
    Profile arena;
    arena.name = "Skyforge Arena";
    arena.programs << "arena.exe";
    arena.remaps << Remap{vk::KeyA + ('Q' - 'A'), KeyCombo{{}, vk::KeyA + ('G' - 'A')}, true}
                 << Remap{vk::F5, KeyCombo{{vk::Ctrl, vk::Shift}, vk::KeyA + ('S' - 'A')}, true}
                 << Remap{vk::Oem102, KeyCombo{{}, vk::Tab}, false};
    Macro build;
    build.name = lang == "de" ? "Schnell bauen" : "Quick build";
    build.trigger = vk::F8;
    build.steps = {{MacroStep::Tap, vk::KeyA + ('B' - 'A'), 0}, {MacroStep::Wait, 0, 40}, {MacroStep::Tap, vk::Key0 + 2, 0},
                   {MacroStep::Wait, 0, 40}, {MacroStep::Tap, vk::Enter, 0}};
    Macro run;
    run.name = lang == "de" ? "Dauerlauf" : "Auto run";
    run.trigger = vk::Oem3;
    run.mode = Macro::WhileHeld;
    run.steps = {{MacroStep::Down, vk::Shift, 0}, {MacroStep::Down, vk::KeyA + ('W' - 'A'), 0}, {MacroStep::Wait, 0, 250},
                 {MacroStep::Up, vk::KeyA + ('W' - 'A'), 0}, {MacroStep::Up, vk::Shift, 0}};
    arena.macros << build << run;
    Profile racer;
    racer.name = "Velocity Rally";
    racer.programs << "rally*.exe";
    racer.remaps << Remap{vk::Space, KeyCombo{{}, vk::KeyA + ('H' - 'A')}, true};
    c.profiles << arena << racer;
    return c;
}

static void grabTo(QWidget* w, const QString& path) {
    w->setAttribute(Qt::WA_DontShowOnScreen);
    w->show();
    for (int i = 0; i < 5; ++i) QApplication::processEvents();
    w->grab().save(path);
}

// Spielszene mit Overlay (für die Website)
static QImage overlayScene(const Metrics& m, OverlayOptions o, const QColor& accent, const QSize& size) {
    QImage img(size, QImage::Format_RGB32);
    QPainter p(&img);
    paintGameScene(p, QRectF(QPointF(0, 0), size));
    const QList<OverlayRow> rows = overlayRows(m, o, QTime(21, 37));
    const QRect place = placeOverlay(overlaySize(rows, o), img.rect(), o.corner, o.margin + 6);
    paintOverlay(p, place.topLeft(), rows, o, accent);
    return img;
}

// ---------------------------------------------------------------- Website-Bilder: --siteshots <ordner>
static int siteShots(const QString& dir) {
    QDir().mkpath(dir);
    g_demoUi = true;
    for (const QString& lang : {QString("de"), QString("en")}) {
        Config c = demoConfig(lang);
        setLanguage(lang);
        App app(c, true);
        app.applyLook();
        app.setMetricsForTest(demoMetrics());
        MainWindow win(&app);
        win.resize(1120, 740);
        win.setAttribute(Qt::WA_DontShowOnScreen);
        win.show();
        win.fillDemoHistory();
        win.setEditProfile(1);
        const char* names[] = {"overview", "overlay", "keys", "macros", "profiles", "settings"};
        for (int i = 0; i < PageCount; ++i) {
            win.showPage(i);
            for (int k = 0; k < 5; ++k) QApplication::processEvents();
            win.grab().save(QString("%1/%2-%3.png").arg(dir, names[i], lang));
        }
        // Makro-Editor
        MacroDialog dlg(&app, app.config().profiles[1].macros[0], &win);
        dlg.resize(560, 560);
        grabTo(&dlg, QString("%1/macro-edit-%2.png").arg(dir, lang));
        // Overlay im Spiel: groß und Zeile
        OverlayOptions o = app.config().overlay;
        o.scale = 150;
        o.items[int(Item::Clock)] = true;
        overlayScene(demoMetrics(), o, QColor(c.accent), QSize(1280, 720)).save(QString("%1/game-%2.png").arg(dir, lang));
        o.horizontal = true;
        o.corner = Corner::TopRight;
        o.scale = 130;
        overlayScene(demoMetrics(), o, QColor(c.accent), QSize(1280, 720)).save(QString("%1/game-line-%2.png").arg(dir, lang));
        // Hell
        app.config().dark = false;
        app.changed();
        win.rebuild();
        win.fillDemoHistory();
        win.showPage(PageOverview);
        for (int k = 0; k < 5; ++k) QApplication::processEvents();
        win.grab().save(QString("%1/overview-light-%2.png").arg(dir, lang));
    }
    // Logo
    syslineLogo(512).save(dir + "/icon.png");
    return 0;
}

// ---------------------------------------------------------------- Selbsttest: --selftest <ordner>
namespace {
QFile g_log;
int g_fail = 0, g_pass = 0;
void say(const QString& s) {
    g_log.write((s + '\n').toUtf8());
    g_log.flush();
}
void check(bool ok, const QString& name, const QString& detail = QString()) {
    (ok ? g_pass : g_fail)++;
    say(QString("%1  %2%3").arg(ok ? "PASS" : "FAIL", name, detail.isEmpty() ? QString() : "  (" + detail + ")"));
}
QString outs(const QList<OutKey>& o) {
    QStringList l;
    for (const OutKey& k : o) l << QString("%1%2").arg(k.down ? "+" : "-", keyName(k.key));
    return l.join(' ');
}
QString sched(const QList<TimedKey>& t) {
    QStringList l;
    for (const TimedKey& k : t) l << QString("%1%2@%3").arg(k.down ? "+" : "-", keyName(k.key)).arg(k.at);
    return l.join(' ');
}
int K(char c) { return c >= '0' && c <= '9' ? vk::Key0 + (c - '0') : vk::KeyA + (c - 'A'); }
}  // namespace

static void testKeys() {
    bool all = true;
    QString bad;
    for (int k : allKeys())
        if (keyFromName(keyName(k)) != k) { all = false; bad += keyName(k) + " "; }
    check(all, "keys: every key name reads back", bad);
    check(keyFromName("strg") == vk::Ctrl && keyFromName("Leertaste") == vk::Space && keyFromName("f12") == vk::F1 + 11,
          "keys: German and lower-case names");
    const KeyCombo c = comboFromName("Ctrl+Shift+C");
    check(c.mods == QList<int>{vk::Ctrl, vk::Shift} && c.key == K('C'), "keys: combo Ctrl+Shift+C", comboName(c));
    check(comboName(comboFromName(comboName(c))) == "Ctrl+Shift+C", "keys: combo round trip");
    check(comboFromName("Off").key == 0 && comboFromName("Off").mods.isEmpty(), "keys: switched off");
    check(keyFromQt(Qt::Key_5, true) == vk::Num0 + 5 && keyFromQt(Qt::Key_5, false) == K('5'), "keys: number pad vs digits");
    check(genericKey(vk::RShift) == vk::Shift && isExtendedKey(vk::Right) && !isExtendedKey(K('A')), "keys: generic / extended");
}

static void testRemap() {
    Profile p;
    p.remaps << Remap{vk::CapsLock, KeyCombo{{}, vk::Ctrl}, true} << Remap{K('A'), KeyCombo{{vk::Ctrl}, K('C')}, true}
             << Remap{vk::LWin, KeyCombo{}, true} << Remap{K('B'), KeyCombo{{}, K('X')}, false}
             << Remap{vk::Shift, KeyCombo{{}, K('S')}, true} << Remap{vk::RShift, KeyCombo{{}, K('R')}, true};
    KeyEngine e;
    e.setProfile(p, true, false);
    KeyDecision d = e.onKey(vk::CapsLock, true);
    check(d.block && outs(d.out) == "+Ctrl", "remap: CapsLock down -> Ctrl down", outs(d.out));
    d = e.onKey(vk::CapsLock, true);
    check(d.block && outs(d.out) == "+Ctrl", "remap: key repeat repeats target", outs(d.out));
    d = e.onKey(vk::CapsLock, false);
    check(d.block && outs(d.out) == "-Ctrl", "remap: CapsLock up -> Ctrl up", outs(d.out));
    d = e.onKey(K('A'), true);
    check(outs(d.out) == "+Ctrl +C", "remap: A -> Ctrl+C presses modifier first", outs(d.out));
    d = e.onKey(K('A'), false);
    check(outs(d.out) == "-C -Ctrl", "remap: A up releases in reverse", outs(d.out));
    d = e.onKey(vk::LWin, true);
    check(d.block && d.out.isEmpty(), "remap: Win key switched off");
    e.onKey(vk::LWin, false);
    d = e.onKey(K('B'), true);
    check(!d.block && d.out.isEmpty(), "remap: disabled rule passes key through");
    e.onKey(K('B'), false);
    d = e.onKey(K('Z'), true);
    check(!d.block, "remap: other keys untouched");
    d = e.onKey(K('Z'), false);
    check(!d.block, "remap: untouched key up passes");
    d = e.onKey(vk::LShift, true);
    check(outs(d.out) == "+S", "remap: generic Shift matches left Shift", outs(d.out));
    e.onKey(vk::LShift, false);
    d = e.onKey(vk::RShift, true);
    check(outs(d.out) == "+R", "remap: exact right Shift wins over generic", outs(d.out));
    e.onKey(vk::RShift, false);
    // Profilwechsel, während eine Taste gedrückt ist: Loslassen passt trotzdem
    e.onKey(K('A'), true);
    e.setProfile(Profile{}, true, false);
    d = e.onKey(K('A'), false);
    check(d.block && outs(d.out) == "-C -Ctrl", "remap: profile switch while held still releases", outs(d.out));
    // Global aus
    e.setProfile(p, false, false);
    check(!e.onKey(vk::CapsLock, true).block, "remap: paused globally");
    e.onKey(vk::CapsLock, false);
    p.remapOn = false;
    e.setProfile(p, true, false);
    check(!e.onKey(vk::CapsLock, true).block, "remap: off in this profile");
    e.onKey(vk::CapsLock, false);
    // releaseAll
    p.remapOn = true;
    e.setProfile(p, true, false);
    e.onKey(K('A'), true);
    QList<OutKey> rel;
    e.releaseAll(&rel);
    check(outs(rel) == "-C -Ctrl", "remap: release everything on stop", outs(rel));
}

static void testMacros() {
    QList<MacroStep> s = {{MacroStep::Tap, K('A'), 0}, {MacroStep::Wait, 0, 50}, {MacroStep::Down, vk::Shift, 0},
                          {MacroStep::Tap, K('B'), 0}, {MacroStep::Up, vk::Shift, 0}};
    int total = 0;
    QList<TimedKey> t = macroSchedule(s, 1, &total);
    check(sched(t) == "+A@0 -A@20 +Shift@70 +B@70 -B@90 -Shift@90" && total == 90, "macro: schedule with taps and pause",
          sched(t) + QString(" total=%1").arg(total));
    t = macroSchedule(s, 2, &total);
    check(t.size() == 12 && t[6].at == 90 && t.last().at == 180 && total == 180, "macro: repeat twice", sched(t));
    t = macroSchedule({{MacroStep::Down, K('W'), 0}, {MacroStep::Wait, 0, 100}}, 1, &total);
    check(sched(t) == "+W@0 -W@100", "macro: still-held key released at the end", sched(t));
    check(macroSchedule({}, 3, &total).isEmpty() && total == 0, "macro: empty");
    // Aufnahme
    QList<RecordedKey> rec = {{1000, K('A'), true}, {1030, K('A'), false}, {1131, vk::Shift, true}, {1180, vk::Shift, true},
                              {1200, K('B'), true}, {1290, K('B'), false}, {1300, vk::Shift, false}, {1400, K('C'), false}};
    QList<MacroStep> st = stepsFromRecording(rec);
    const QString got = formatSteps(st, false).replace('\n', " | ");
    check(got == "tap A | wait 100 | down Shift | wait 70 | down B | wait 90 | up B | wait 10 | up Shift",
          "macro: recording to steps", got);
    rec = {{0, K('Q'), true}, {500, K('Q'), true}};
    st = stepsFromRecording(rec);
    check(st.size() == 2 && st.last().kind == MacroStep::Up, "macro: recording ends with release");
    // Text
    const QString de = formatSteps(s, true);
    check(de.startsWith("tippen A\nwarten 50\ndrücken Shift"), "macro: German text", QString(de).replace('\n', " | "));
    check(parseSteps(de) == s && parseSteps(formatSteps(s, false)) == s, "macro: text round trip de/en");
    int bad = 0;
    parseSteps("tap A\nwait 20\nbanana X\n", &bad);
    check(bad == 3, "macro: wrong line is reported", QString::number(bad));
    const QList<MacroStep> shortForm = parseSteps("A\n50ms\n// Kommentar\nwarten 10 ms\nloslassen Strg");
    check(shortForm.size() == 4 && shortForm[0].kind == MacroStep::Tap && shortForm[1].ms == 50 && shortForm[2].ms == 10 &&
              shortForm[3].kind == MacroStep::Up && shortForm[3].key == vk::Ctrl,
          "macro: short forms and comments");
    // Auslöser über die KeyEngine
    Profile p;
    Macro m;
    m.trigger = vk::F8;
    m.steps = s;
    Macro h = m;
    h.trigger = vk::F9;
    h.mode = Macro::WhileHeld;
    p.macros << m << h;
    p.remaps << Remap{vk::F8, KeyCombo{{}, K('X')}, true};
    KeyEngine e;
    e.setProfile(p, true, false);
    check(!e.onKey(vk::F9, true).block, "macro: off by default (not allowed)");
    e.onKey(vk::F9, false);
    e.setProfile(p, true, true);
    KeyDecision d = e.onKey(vk::F8, true);
    check(d.block && d.startMacro == 0 && d.out.isEmpty(), "macro: trigger starts macro (before remap)");
    d = e.onKey(vk::F8, true);
    check(d.block && d.startMacro < 0, "macro: key repeat doesn't restart");
    d = e.onKey(vk::F8, false);
    check(d.block && d.stopMacro < 0, "macro: once-macro keeps running after release");
    e.onKey(vk::F9, true);
    d = e.onKey(vk::F9, false);
    check(d.stopMacro == 1, "macro: while-held stops on release");
    p.macros[0].enabled = false;
    e.setProfile(p, true, true);
    d = e.onKey(vk::F8, true);
    check(d.startMacro < 0 && outs(d.out) == "+X", "macro: disabled macro falls back to remap", outs(d.out));
    e.onKey(vk::F8, false);
#ifndef Q_OS_WIN
    // Abspielen (sendet hier nichts): läuft, endet und stoppt sauber
    Macro loop = h;
    MacroPlayer::instance().play(loop, vk::F9);
    QElapsedTimer tm;
    tm.start();
    while (MacroPlayer::instance().running() == 0 && tm.elapsed() < 1000) QThread::msleep(5);
    const bool started = MacroPlayer::instance().running() == 1;
    MacroPlayer::instance().stop(vk::F9);
    while (MacroPlayer::instance().running() > 0 && tm.elapsed() < 3000) QThread::msleep(5);
    check(started && MacroPlayer::instance().running() == 0, "macro: player starts and stops (while held)");
    MacroPlayer::instance().stopAll();
#endif
    check(macroSummary(m).contains("0.09") || macroSummary(m).contains("0,09"), "macro: summary shows duration", macroSummary(m));
}

static void testProfiles() {
    QList<Profile> ps(4);
    ps[0].name = "Standard";
    ps[1].programs << "game.exe";
    ps[2].programs << "*-Win64-Shipping.exe" << "racer";
    ps[3].programs << "GAME.EXE";
    check(pickProfile(ps, "C:/Games/X/game.exe") == 1, "profile: exact name with path");
    check(pickProfile(ps, "C:\\Games\\X\\Game.EXE") == 1 || pickProfile(ps, "Game.EXE") == 1, "profile: case insensitive");
    check(pickProfile(ps, "D:/S/Survivor-Win64-Shipping.exe") == 2, "profile: wildcard");
    check(pickProfile(ps, "/usr/bin/racer") == 2 && pickProfile(ps, "C:/racer.exe") == 2, "profile: name without .exe");
    check(pickProfile(ps, "C:/Windows/explorer.exe") == 0, "profile: falls back to Standard");
    check(pickProfile(ps, "") == 0 && pickProfile({}, "game.exe") == 0, "profile: empty");
    check(!programMatches("game.exe", "C:/x/endgame.exe"), "profile: no partial match");
}

static void testOverlay() {
    OverlayOptions o;
    Metrics m = demoMetrics();
    QList<OverlayRow> rows = overlayRows(m, o, QTime(21, 37));
    QStringList labels;
    for (const OverlayRow& r : rows) labels << r.label;
    check(labels.join(',') == "FPS,CPU,GPU,RAM,VRAM,PING", "overlay: default rows", labels.join(','));
    check(rows[0].value == "144" && rows[2].value == "96 %  64 °C", "overlay: values", rows[0].value + " | " + rows[2].value);
    m.fpsState = FpsState::NeedsAdmin;
    rows = overlayRows(m, o, QTime(21, 37));
    check(rows[0].value == L("admin rights needed"), "overlay: honest FPS without admin rights", rows[0].value);
    m.gpuTemp = 91;
    m.ping = 140;
    rows = overlayRows(m, o, QTime(21, 37));
    check(rows[2].hot && rows[5].hot && !rows[1].hot, "overlay: hot values marked");
    for (bool& b : o.items) b = true;
    m.battery = -1;
    rows = overlayRows(m, o, QTime(21, 37));
    check(rows.size() == 7, "overlay: all items, no battery row without battery", QString::number(rows.size()));
    m.battery = 50;
    check(overlayRows(m, o, QTime(9, 5)).size() == 8 && overlayRows(m, o, QTime(9, 5))[6].value == "09:05", "overlay: clock and battery");
    for (bool& b : o.items) b = false;
    check(overlayRows(m, o, QTime()).isEmpty() && overlaySize({}, o).isEmpty(), "overlay: nothing selected");
    o = OverlayOptions();
    rows = overlayRows(demoMetrics(), o, QTime(21, 37));
    const QSize s100 = overlaySize(rows, o);
    o.scale = 200;
    const QSize s200 = overlaySize(rows, o);
    check(s100.width() > 60 && s100.height() > 60 && s200.width() > s100.width() * 17 / 10 && s200.height() > s100.height() * 17 / 10,
          "overlay: size grows with scale", QString("%1x%2 -> %3x%4").arg(s100.width()).arg(s100.height()).arg(s200.width()).arg(s200.height()));
    o.scale = 100;
    o.horizontal = true;
    const QSize line = overlaySize(rows, o);
    check(line.width() > line.height() * 4 && line.height() < s100.height(), "overlay: one line is wide and flat",
          QString("%1x%2").arg(line.width()).arg(line.height()));
    const QRect scr(0, 0, 1920, 1080);
    const QSize sz(200, 120);
    check(placeOverlay(sz, scr, Corner::TopLeft, 14) == QRect(14, 14, 200, 120), "overlay: top left");
    check(placeOverlay(sz, scr, Corner::TopRight, 14) == QRect(1706, 14, 200, 120), "overlay: top right");
    check(placeOverlay(sz, scr, Corner::BottomLeft, 14) == QRect(14, 946, 200, 120), "overlay: bottom left");
    check(placeOverlay(sz, scr, Corner::BottomRight, 14) == QRect(1706, 946, 200, 120), "overlay: bottom right");
    check(placeOverlay(sz, QRect(1920, 0, 2560, 1440), Corner::TopRight, 10) == QRect(4270, 10, 200, 120), "overlay: second screen");
    check(QRect(0, 0, 150, 100).contains(placeOverlay(sz, QRect(0, 0, 150, 100), Corner::BottomRight, 50)) == false &&
              placeOverlay(sz, QRect(0, 0, 150, 100), Corner::BottomRight, 50).topLeft() == QPoint(0, 0),
          "overlay: never outside a tiny screen");
    // Zeichnen: Pixel in der Akzentfarbe links und heller Text
    OverlayWindow ow;
    o = OverlayOptions();
    ow.setOptions(o, QColor("#d61f45"));
    ow.setMetrics(demoMetrics());
    const QImage img = ow.render();
    bool accent = false, bright = false;
    for (int y = 0; y < img.height(); ++y) {
        const QColor c = img.pixelColor(1, y);
        accent |= c.red() > 150 && c.green() < 80;
        for (int x = img.width() / 2; x < img.width(); ++x) bright |= qGray(img.pixel(x, y)) > 220;
    }
    check(!img.isNull() && accent && bright, "overlay: rendered with accent edge and readable text",
          QString("%1x%2").arg(img.width()).arg(img.height()));
    check(ow.testAttribute(Qt::WA_TranslucentBackground) && (ow.windowFlags() & Qt::WindowTransparentForInput) &&
              (ow.windowFlags() & Qt::WindowStaysOnTopHint),
          "overlay: window is see-through, click-through, on top");
}

static void testConfig(const QString& dir) {
    const QString path = dir + "/settings-test.ini";
    QFile::remove(path);
    Config c = demoConfig("de");
    c.overlay.corner = Corner::BottomRight;
    c.overlay.scale = 140;
    c.overlay.opacity = 30;
    c.overlay.horizontal = true;
    c.overlay.items[int(Item::Clock)] = true;
    c.overlay.hotkey = "Ctrl+Alt+F10";
    c.pingHost = "example.org";
    c.accent = "#3b82f6";
    c.acryl = true;
    c.autoProfile = false;
    c.manualProfile = 2;
    {
        QSettings s(path, QSettings::IniFormat);
        c.save(s);
    }
    QSettings s(path, QSettings::IniFormat);
    const Config r = Config::load(s);
    check(r.language == "de" && r.accent == "#3b82f6" && r.acryl && !r.autoProfile && r.manualProfile == 2, "config: general values");
    check(r.overlay.corner == Corner::BottomRight && r.overlay.scale == 140 && r.overlay.opacity == 30 && r.overlay.horizontal &&
              r.overlay.on(Item::Clock) && r.overlay.hotkey == "Ctrl+Alt+F10",
          "config: overlay values");
    check(r.pingHost == "example.org" && r.macrosAllowed, "config: ping host and macros");
    bool same = r.profiles.size() == c.profiles.size();
    for (int i = 0; same && i < r.profiles.size(); ++i) {
        const Profile &a = c.profiles[i], &b = r.profiles[i];
        same = a.name == b.name && a.programs == b.programs && a.remaps.size() == b.remaps.size() && a.macros.size() == b.macros.size();
        for (int k = 0; same && k < a.remaps.size(); ++k)
            same = a.remaps[k].from == b.remaps[k].from && a.remaps[k].to == b.remaps[k].to && a.remaps[k].enabled == b.remaps[k].enabled;
        for (int k = 0; same && k < a.macros.size(); ++k)
            same = a.macros[k].steps == b.macros[k].steps && a.macros[k].trigger == b.macros[k].trigger &&
                   a.macros[k].mode == b.macros[k].mode && a.macros[k].name == b.macros[k].name;
    }
    check(same, "config: profiles with remaps and macros round trip");
    QSettings empty(dir + "/settings-empty.ini", QSettings::IniFormat);
    empty.clear();
    const Config d = Config::load(empty);
    check(d.profiles.size() == 1 && d.profiles[0].name == "Standard" && !d.macrosAllowed && d.pingHost == "mert.win" &&
              d.overlay.hotkey == "Ctrl+Shift+O" && d.accent == "#d61f45",
          "config: defaults (macros off, ping mert.win, Ctrl+Shift+O, carmine)");
    s.setValue("overlay/scale", 9999);
    s.setValue("look/accent", "not-a-colour");
    const Config bad = Config::load(s);
    check(bad.overlay.scale == 200 && bad.accent == "#d61f45", "config: bad values are clamped");
}

static void testHelpers() {
    FpsCounter f;
    for (int i = 0; i < 288; ++i) f.add(100, 0, 10.0 + i / 144.0);
    const double fps = f.fps(100, 12.2);
    check(std::abs(fps - 144) < 2, "fps: 144 frames per second", QString::number(fps, 'f', 1));
    check(f.fps(100, 20) < 0 && f.fps(7, 12) < 0, "fps: stale or unknown process");
    for (int i = 0; i < 120; ++i) f.add(200, 2, 10.0 + i / 60.0);
    for (int i = 0; i < 60; ++i) f.add(200, 0, 10.0 + i / 30.0);
    check(std::abs(f.fps(200, 12.0) - 30) < 2, "fps: prefers DXGI over kernel events", QString::number(f.fps(200, 12.0), 'f', 1));
    f.prune(30);
    check(f.fps(100, 30) < 0, "fps: prune");
    const QList<QPair<QString, double>> eng = {
        {"pid_100_luid_0x00000000_0x0000C3F2_phys_0_eng_0_engtype_3D", 60}, {"pid_200_luid_0x00000000_0x0000C3F2_phys_0_eng_0_engtype_3D", 25},
        {"pid_100_luid_0x00000000_0x0000C3F2_phys_0_eng_3_engtype_Copy", 40}, {"pid_300_luid_0x00000000_0x0000AAAA_phys_0_eng_0_engtype_3D", 99}};
    check(gpuLoadFromEngines(eng, "luid_0x00000000_0x0000C3F2") == 85, "gpu: engine sum per adapter (like Task Manager)");
    check(gpuLoadFromEngines(eng) == 99 && gpuLoadFromEngines({}) < 0, "gpu: all adapters / none");
    check(parsePingOutput("64 bytes from x: icmp_seq=1 ttl=57 time=13.2 ms") == 13.2, "ping: Linux output");
    check(parsePingOutput("Antwort von x: Bytes=32 Zeit=14ms TTL=57") == 14, "ping: German Windows output");
    check(parsePingOutput("Reply from x: bytes=32 time<1ms TTL=128") <= 0.5 && parsePingOutput("timeout") < 0, "ping: <1 ms and failure");
    quint64 t = 0, i = 0;
    check(parseProcStat("cpu  100 0 50 800 50 0 0 0 0 0", &t, &i) && t == 1000 && i == 850, "linux: /proc/stat");
    double used = 0, total = 0;
    check(parseMemInfo("MemTotal:       16000000 kB\nMemFree: 1 kB\nMemAvailable:    6000000 kB\n", &used, &total) &&
              std::abs(total - 15.26) < 0.01 && std::abs(used - 9.54) < 0.01,
          "linux: /proc/meminfo");
}

static void testWidgets() {
    HotkeyButton hk("Ctrl+Shift+O");
    QString got;
    QObject::connect(&hk, &HotkeyButton::sequenceChanged, [&](const QString& s) { got = s; });
    hk.pressForTest(Qt::Key_A, Qt::NoModifier);
    check(got.isEmpty(), "ui: overlay key A alone is refused");
    hk.pressForTest(Qt::Key_O, Qt::ControlModifier | Qt::AltModifier);
    check(got == "Ctrl+Alt+O", "ui: overlay key Ctrl+Alt+O", got);
    hk.pressForTest(Qt::Key_F10, Qt::NoModifier);
    check(got == "F10", "ui: F-key alone is allowed", got);
    KeyButton kb(KeyCombo{}, true);
    KeyCombo kc;
    QObject::connect(&kb, &KeyButton::comboChanged, [&](const KeyCombo& c) { kc = c; });
    kb.pressForTest(Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);
    check(kc.key == K('C') && kc.mods == QList<int>{vk::Ctrl, vk::Shift}, "ui: key field takes Ctrl+Shift+C", comboName(kc));
    KeyButton single(KeyCombo{}, false);
    single.pressForTest(Qt::Key_Q, Qt::ControlModifier);
    check(single.combo().key == K('Q') && single.combo().mods.isEmpty(), "ui: single key field ignores modifiers");
}

static int selfTest(const QString& dir) {
    QDir().mkpath(dir);
    g_log.setFileName(dir + "/selftest.txt");
    g_log.open(QIODevice::WriteOnly | QIODevice::Text);
    say(QString("Sysline %1 selftest, Qt %2, %3, platform %4").arg(APP_VERSION, qVersion(), QSysInfo::prettyProductName(),
                                                                   QGuiApplication::platformName()));
    setLanguage("en");
    testKeys();
    testRemap();
    testMacros();
    testProfiles();
    testOverlay();
    testConfig(dir);
    testHelpers();
    testWidgets();

    // Echte Werte dieses Rechners einmal lesen (nur protokolliert - in der CI gibt es keine Grafikkarte)
    {
        auto reader = makeSystemReader();
        Metrics m;
        reader->read(m);
        QThread::msleep(300);
        reader->read(m);
        say(QString("info: cpu=%1 ram=%2/%3 gpu=%4 vram=%5/%6 gputemp=%7 cputemp=%8 battery=%9 gpuName=%10")
                .arg(m.cpu).arg(m.ramUsed).arg(m.ramTotal).arg(m.gpu).arg(m.vramUsed).arg(m.vramTotal).arg(m.gpuTemp)
                .arg(m.cpuTemp).arg(m.battery).arg(m.gpuName));
        check(m.ramTotal > 0.2 && m.ramUsed > 0 && m.ramUsed <= m.ramTotal, "system: RAM read");
        check(m.cpu >= 0 && m.cpu <= 100, "system: CPU load read", QString::number(m.cpu));
        qint64 pid = 0;
        const QString fg = foregroundProgram(&pid);
        say("info: foreground=" + fg + QString(" pid=%1").arg(pid));
        say("info: cpu temp note: " + Sampler::cpuTempNote());
    }

    // Alle Seiten unsichtbar zeichnen, auf Englisch und Deutsch, dunkel und hell; danach: fehlt eine Übersetzung?
    for (const QString& lang : {QString("en"), QString("de")}) {
        for (bool dark : {true, false}) {
            Config c = demoConfig(lang);
            c.dark = dark;
            c.acryl = !dark && lang == "de";
            App app(c, true);
            app.setMetricsForTest(demoMetrics());
            MainWindow win(&app);
            win.resize(1080, 720);
            win.setAttribute(Qt::WA_DontShowOnScreen);
            win.show();
            win.setEditProfile(1);
            bool ok = true;
            for (int i = 0; i < PageCount; ++i) {
                win.showPage(i);
                QApplication::processEvents();
                const QImage img = win.grab().toImage();
                ok &= !img.isNull() && img.width() >= 1000;
                if (dark) img.save(QString("%1/page%2-%3.png").arg(dir).arg(i).arg(lang));
            }
            check(ok && win.stack()->count() == PageCount, QString("ui: all pages render (%1, %2)").arg(lang, dark ? "dark" : "light"));
            check(app.activeProfile() == 1, "ui: profile picked from program in front", QString::number(app.activeProfile()));
            MacroDialog dlg(&app, app.config().profiles[1].macros[0], &win);
            dlg.setAttribute(Qt::WA_DontShowOnScreen);
            dlg.show();
            QApplication::processEvents();
            check(dlg.macro().steps == app.config().profiles[1].macros[0].steps, "ui: macro editor keeps steps (" + lang + ")");
            if (dark) dlg.grab().save(QString("%1/macro-%2.png").arg(dir, lang));
            // Sprache wechseln baut alles neu auf
            app.config().language = lang == "de" ? "en" : "de";
            app.changed();
            win.rebuild();
            QApplication::processEvents();
            check(win.stack()->count() == PageCount, "ui: rebuild after language change");
        }
    }
    setLanguage("de");
    const QStringList missing = missingGerman();
    check(missing.isEmpty(), "i18n: every text shown has a German version", missing.join(" | "));
    say(QString("done: %1 passed, %2 failed").arg(g_pass).arg(g_fail));
    return g_fail ? 1 : 0;
}

// ---------------------------------------------------------------- Start
int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
    SetCurrentProcessExplicitAppUserModelID(L"WSoftware.Sysline");
#endif
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("WSoftware");
    QCoreApplication::setApplicationName("Sysline");
    QGuiApplication::setApplicationDisplayName("Sysline");
#ifdef Q_OS_LINUX
    QGuiApplication::setDesktopFileName(kLinuxAppId);
#endif
    app.setWindowIcon(syslineIcon());
    app.setQuitOnLastWindowClosed(false);
    installNoWheelChange();

    const QStringList args = app.arguments();
    if (const int i = args.indexOf("--selftest"); i >= 0 && i + 1 < args.size()) return selfTest(args[i + 1]);
    // Symbole für die Pakete: PNGs in allen Größen (tools/make_icons.py baut daraus .ico und .icns)
    if (const int i = args.indexOf("--icons"); i >= 0 && i + 1 < args.size()) {
        QDir().mkpath(args[i + 1]);
        for (int s : {16, 24, 32, 48, 64, 128, 256, 512, 1024}) syslineLogo(s).save(QString("%1/icon-%2.png").arg(args[i + 1]).arg(s));
        return 0;
    }
    if (const int i = args.indexOf("--siteshots"); i >= 0 && i + 1 < args.size()) return siteShots(args[i + 1]);
    // Neustart mit Adminrechten: warten, bis die alte Instanz weg ist
    if (const int i = args.indexOf("--after"); i >= 0 && i + 1 < args.size()) waitForProcessExit(args[i + 1].toLongLong(), 8000);
    const bool background = args.contains("--background");
    if (args.contains("--toggle-overlay")) return sendToRunning("overlay") ? 0 : 1;
    if (sendToRunning(background ? "noop" : "show")) return 0;

    Config cfg = Config::load();
    setLanguage(cfg.language);
    const bool first = !cfg.firstRunDone;
    cfg.firstRunDone = true;
    App sys(cfg);
    sys.listen();
    if (!background || first) QTimer::singleShot(0, &sys, &App::showWindow);
    return app.exec();
}
