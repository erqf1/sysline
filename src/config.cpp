#include "config.h"

#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include "i18n.h"

QString itemKey(Item i) {
    static const char* k[] = {"fps", "cpu", "cputemp", "gpu", "gputemp", "ram", "vram", "ping", "clock", "battery"};
    return QString::fromLatin1(k[int(i)]);
}

static QJsonObject stepToJson(const MacroStep& s) {
    static const char* kinds[] = {"tap", "down", "up", "wait"};
    QJsonObject o{{"do", kinds[int(s.kind)]}};
    if (s.kind == MacroStep::Wait) o["ms"] = s.ms;
    else o["key"] = keyName(s.key);
    return o;
}

static MacroStep stepFromJson(const QJsonObject& o) {
    MacroStep s;
    const QString k = o.value("do").toString();
    s.kind = k == "down" ? MacroStep::Down : k == "up" ? MacroStep::Up : k == "wait" ? MacroStep::Wait : MacroStep::Tap;
    s.ms = std::clamp(o.value("ms").toInt(), 0, 600000);
    s.key = keyFromName(o.value("key").toString());
    return s;
}

QJsonArray profilesToJson(const QList<Profile>& profiles) {
    QJsonArray out;
    for (const Profile& p : profiles) {
        QJsonArray remaps, macros;
        for (const Remap& r : p.remaps)
            remaps << QJsonObject{{"from", keyName(r.from)}, {"to", comboName(r.to)}, {"on", r.enabled}};
        for (const Macro& m : p.macros) {
            QJsonArray steps;
            for (const MacroStep& s : m.steps) steps << stepToJson(s);
            static const char* modes[] = {"once", "repeat", "held"};
            macros << QJsonObject{{"name", m.name}, {"trigger", keyName(m.trigger)}, {"mode", modes[int(m.mode)]},
                                  {"repeat", m.repeat}, {"on", m.enabled}, {"steps", steps}};
        }
        out << QJsonObject{{"name", p.name}, {"programs", QJsonArray::fromStringList(p.programs)}, {"remapOn", p.remapOn},
                           {"macrosOn", p.macrosOn}, {"remaps", remaps}, {"macros", macros}};
    }
    return out;
}

QList<Profile> profilesFromJson(const QJsonArray& a) {
    QList<Profile> out;
    for (const QJsonValue& v : a) {
        const QJsonObject o = v.toObject();
        Profile p;
        p.name = o.value("name").toString();
        for (const QJsonValue& x : o.value("programs").toArray())
            if (!x.toString().trimmed().isEmpty()) p.programs << x.toString().trimmed();
        p.remapOn = o.value("remapOn").toBool(true);
        p.macrosOn = o.value("macrosOn").toBool(true);
        for (const QJsonValue& x : o.value("remaps").toArray()) {
            const QJsonObject r = x.toObject();
            Remap m;
            m.from = keyFromName(r.value("from").toString());
            m.to = comboFromName(r.value("to").toString());
            m.enabled = r.value("on").toBool(true);
            if (m.from) p.remaps << m;
        }
        for (const QJsonValue& x : o.value("macros").toArray()) {
            const QJsonObject r = x.toObject();
            Macro m;
            m.name = r.value("name").toString();
            m.trigger = keyFromName(r.value("trigger").toString());
            const QString mode = r.value("mode").toString();
            m.mode = mode == "repeat" ? Macro::Repeat : mode == "held" ? Macro::WhileHeld : Macro::Once;
            m.repeat = std::clamp(r.value("repeat").toInt(3), 1, 1000);
            m.enabled = r.value("on").toBool(true);
            for (const QJsonValue& s : r.value("steps").toArray()) m.steps << stepFromJson(s.toObject());
            p.macros << m;
        }
        out << p;
    }
    return out;
}

void Config::ensureDefaultProfile() {
    if (profiles.isEmpty()) {
        Profile p;
        p.name = "Standard";
        profiles << p;
    }
    profiles[0].programs.clear();  // Standard gilt immer für alle anderen Programme
    manualProfile = std::clamp(manualProfile, 0, int(profiles.size()) - 1);
}

Config Config::load() {
    QSettings s("WSoftware", "Sysline");
    return load(s);
}

void Config::save() const {
    QSettings s("WSoftware", "Sysline");
    save(s);
}

Config Config::load(QSettings& s) {
    Config c;
    c.language = s.value("language").toString();
    c.autostart = s.value("autostart", false).toBool();
    c.firstRunDone = s.value("firstRunDone", false).toBool();
    c.dark = s.value("look/dark", true).toBool();
    c.accent = s.value("look/accent", c.accent).toString();
    if (!QColor(c.accent).isValid()) c.accent = "#d61f45";
    c.minimal = s.value("look/minimal", false).toBool();
    c.acryl = s.value("look/acryl", false).toBool();

    OverlayOptions& o = c.overlay;
    s.beginGroup("overlay");
    o.visible = s.value("visible", true).toBool();
    o.hotkey = s.value("hotkey", o.hotkey).toString();
    o.corner = Corner(std::clamp(s.value("corner", 0).toInt(), 0, 3));
    o.scale = std::clamp(s.value("scale", 100).toInt(), 70, 200);
    o.opacity = std::clamp(s.value("opacity", o.opacity).toInt(), 0, 100);
    o.horizontal = s.value("horizontal", false).toBool();
    for (int i = 0; i < kItemCount; ++i) o.items[i] = s.value("item/" + itemKey(Item(i)), o.items[i]).toBool();
    o.screen = s.value("screen").toString();
    o.hideFromCapture = s.value("hideFromCapture", false).toBool();
    o.margin = std::clamp(s.value("margin", 14).toInt(), 0, 200);
    s.endGroup();

    c.pingHost = s.value("pingHost", c.pingHost).toString().trimmed();
    c.intervalMs = std::clamp(s.value("intervalMs", 1000).toInt(), 250, 5000);
    c.remapAllowed = s.value("keys/remapAllowed", true).toBool();
    c.macrosAllowed = s.value("keys/macrosAllowed", false).toBool();
    c.autoProfile = s.value("keys/autoProfile", true).toBool();
    c.manualProfile = s.value("keys/manualProfile", 0).toInt();
    c.profiles = profilesFromJson(QJsonDocument::fromJson(s.value("profiles").toByteArray()).array());
    c.ensureDefaultProfile();
    return c;
}

void Config::save(QSettings& s) const {
    s.setValue("language", language);
    s.setValue("autostart", autostart);
    s.setValue("firstRunDone", firstRunDone);
    s.setValue("look/dark", dark);
    s.setValue("look/accent", accent);
    s.setValue("look/minimal", minimal);
    s.setValue("look/acryl", acryl);
    s.beginGroup("overlay");
    s.setValue("visible", overlay.visible);
    s.setValue("hotkey", overlay.hotkey);
    s.setValue("corner", int(overlay.corner));
    s.setValue("scale", overlay.scale);
    s.setValue("opacity", overlay.opacity);
    s.setValue("horizontal", overlay.horizontal);
    for (int i = 0; i < kItemCount; ++i) s.setValue("item/" + itemKey(Item(i)), overlay.items[i]);
    s.setValue("screen", overlay.screen);
    s.setValue("hideFromCapture", overlay.hideFromCapture);
    s.setValue("margin", overlay.margin);
    s.endGroup();
    s.setValue("pingHost", pingHost);
    s.setValue("intervalMs", intervalMs);
    s.setValue("keys/remapAllowed", remapAllowed);
    s.setValue("keys/macrosAllowed", macrosAllowed);
    s.setValue("keys/autoProfile", autoProfile);
    s.setValue("keys/manualProfile", manualProfile);
    s.setValue("profiles", QJsonDocument(profilesToJson(profiles)).toJson(QJsonDocument::Compact));
    s.sync();
}
