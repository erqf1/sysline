#include "engine.h"

#include <QFileInfo>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include "i18n.h"

// ---------------------------------------------------------------- Makro-Zeitplan
QList<TimedKey> macroSchedule(const QList<MacroStep>& steps, int repeats, int* totalMs) {
    QList<TimedKey> out;
    int t = 0;
    QList<int> downKeys;
    repeats = std::max(1, repeats);
    for (int r = 0; r < repeats; ++r) {
        for (const MacroStep& s : steps) {
            switch (s.kind) {
            case MacroStep::Wait:
                t += std::max(0, s.ms);
                break;
            case MacroStep::Down:
                if (!s.key) break;
                out << TimedKey{t, s.key, true};
                if (!downKeys.contains(s.key)) downKeys << s.key;
                break;
            case MacroStep::Up:
                if (!s.key) break;
                out << TimedKey{t, s.key, false};
                downKeys.removeAll(s.key);
                break;
            case MacroStep::Tap:
                if (!s.key) break;
                out << TimedKey{t, s.key, true} << TimedKey{t + kTapHoldMs, s.key, false};
                t += kTapHoldMs;
                break;
            }
        }
    }
    // Was am Ende noch gedrückt ist, wird losgelassen - sonst "klemmt" die Taste im Spiel
    for (int i = int(downKeys.size()) - 1; i >= 0; --i) out << TimedKey{t, downKeys[i], false};
    if (totalMs) *totalMs = t;
    return out;
}

QList<MacroStep> stepsFromRecording(const QList<RecordedKey>& events) {
    QList<MacroStep> steps;
    QSet<int> down;
    qint64 last = -1;
    for (const RecordedKey& e : events) {
        if (!e.key) continue;
        if (e.down && down.contains(e.key)) continue;  // Tastenwiederholung
        if (!e.down && !down.contains(e.key)) continue;  // Loslassen ohne Drücken (Aufnahme mittendrin gestartet)
        if (last >= 0) {
            const int gap = int((e.ms - last + 2) / 5 * 5);  // auf 5 ms runden
            if (gap >= 5) steps << MacroStep{MacroStep::Wait, 0, gap};
        }
        last = e.ms;
        if (e.down) {
            down.insert(e.key);
            steps << MacroStep{MacroStep::Down, e.key, 0};
        } else {
            down.remove(e.key);
            // Drücken, kurze Pause (<= 60 ms), Loslassen derselben Taste -> Tippen
            const int n = int(steps.size());
            if (n >= 1 && steps[n - 1].kind == MacroStep::Down && steps[n - 1].key == e.key) {
                steps[n - 1].kind = MacroStep::Tap;
            } else if (n >= 2 && steps[n - 1].kind == MacroStep::Wait && steps[n - 1].ms <= 60 &&
                       steps[n - 2].kind == MacroStep::Down && steps[n - 2].key == e.key) {
                steps.removeLast();
                steps.last().kind = MacroStep::Tap;
            } else {
                steps << MacroStep{MacroStep::Up, e.key, 0};
            }
        }
    }
    // Noch gedrückt, als die Aufnahme endete: loslassen
    for (int k : down) steps << MacroStep{MacroStep::Up, k, 0};
    return steps;
}

namespace {
struct Verb {
    MacroStep::Kind kind;
    const char* en;
    const char* de;
};
const Verb kVerbs[] = {{MacroStep::Tap, "tap", "tippen"}, {MacroStep::Down, "down", "drücken"},
                       {MacroStep::Up, "up", "loslassen"}, {MacroStep::Wait, "wait", "warten"}};
}  // namespace

QString formatSteps(const QList<MacroStep>& steps, bool german) {
    QStringList lines;
    for (const MacroStep& s : steps) {
        const Verb& v = kVerbs[int(s.kind)];
        const QString verb = QString::fromUtf8(german ? v.de : v.en);
        lines << (s.kind == MacroStep::Wait ? QString("%1 %2").arg(verb).arg(s.ms) : QString("%1 %2").arg(verb, keyName(s.key)));
    }
    return lines.join('\n');
}

QList<MacroStep> parseSteps(const QString& text, int* errorLine) {
    QList<MacroStep> out;
    if (errorLine) *errorLine = 0;
    const QStringList lines = text.split('\n');
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        const int hash = line.indexOf("//");
        if (hash >= 0) line = line.left(hash);
        line = line.trimmed();
        if (line.isEmpty()) continue;
        const int sp = line.indexOf(QRegularExpression("\\s"));
        const QString verb = (sp < 0 ? line : line.left(sp)).toLower();
        const QString arg = sp < 0 ? QString() : line.mid(sp).trimmed();
        int kind = -1;
        for (const Verb& v : kVerbs)
            if (verb == QString::fromUtf8(v.en) || verb == QString::fromUtf8(v.de)) kind = int(v.kind);
        // Kurzformen: "50ms", "50" = warten, sonst ein Tastenname allein = tippen
        if (kind < 0) {
            static const QRegularExpression msOnly("^(\\d+)\\s*ms$");
            const auto m = msOnly.match(line.toLower());
            bool num = false;
            const int n = line.toInt(&num);
            if (m.hasMatch()) out << MacroStep{MacroStep::Wait, 0, m.captured(1).toInt()};
            else if (num && n >= 0) out << MacroStep{MacroStep::Wait, 0, n};
            else if (const int k = keyFromName(line)) out << MacroStep{MacroStep::Tap, k, 0};
            else {
                if (errorLine && !*errorLine) *errorLine = i + 1;
            }
            continue;
        }
        if (kind == MacroStep::Wait) {
            QString a = arg.toLower();
            if (a.endsWith("ms")) a.chop(2);
            bool ok = false;
            const int ms = a.trimmed().toInt(&ok);
            if (ok && ms >= 0 && ms <= 600000) out << MacroStep{MacroStep::Wait, 0, ms};
            else if (errorLine && !*errorLine) *errorLine = i + 1;
        } else {
            const int k = keyFromName(arg);
            if (k) out << MacroStep{MacroStep::Kind(kind), k, 0};
            else if (errorLine && !*errorLine) *errorLine = i + 1;
        }
    }
    return out;
}

QString macroSummary(const Macro& m) {
    int total = 0;
    macroSchedule(m.steps, 1, &total);
    const QString mode = m.mode == Macro::Once ? L("once")
                         : m.mode == Macro::Repeat ? L("%1 times").arg(m.repeat)
                                                   : L("while held");
    return L("%1 steps · %2 s · %3").arg(m.steps.size()).arg(QLocale(language() == "de" ? QLocale::German : QLocale::English).toString(total / 1000.0, 'f', 2), mode);
}

// ---------------------------------------------------------------- Profile
bool programMatches(const QString& pattern, const QString& exePath) {
    const QString p = pattern.trimmed();
    if (p.isEmpty() || exePath.isEmpty()) return false;
    const QString name = QFileInfo(exePath).fileName();
    if (p.contains('*') || p.contains('?')) {
        const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(p, QRegularExpression::UnanchoredWildcardConversion),
                                    QRegularExpression::CaseInsensitiveOption);
        const QRegularExpression anchored("^(?:" + re.pattern() + ")$", QRegularExpression::CaseInsensitiveOption);
        return anchored.match(name).hasMatch();
    }
    // Ohne ".exe" geschrieben (z. B. auf Linux/macOS) passt auch
    return name.compare(p, Qt::CaseInsensitive) == 0 || QFileInfo(name).completeBaseName().compare(p, Qt::CaseInsensitive) == 0 ||
           name.compare(p + ".exe", Qt::CaseInsensitive) == 0;
}

int pickProfile(const QList<Profile>& profiles, const QString& exePath) {
    for (int i = 1; i < profiles.size(); ++i)
        for (const QString& p : profiles[i].programs)
            if (programMatches(p, exePath)) return i;
    return 0;
}

// ---------------------------------------------------------------- Tasten-Entscheidung
void KeyEngine::setProfile(const Profile& p, bool remapAllowed, bool macrosAllowed) {
    std::lock_guard<std::mutex> lock(mu_);
    remaps_.clear();
    macros_.clear();
    if (remapAllowed && p.remapOn)
        for (const Remap& r : p.remaps)
            if (r.enabled && r.from) remaps_ << r;
    if (macrosAllowed && p.macrosOn) macros_ = p.macros;
}

Macro KeyEngine::macro(int index) const {
    std::lock_guard<std::mutex> lock(mu_);
    return index >= 0 && index < macros_.size() ? macros_[index] : Macro{};
}

KeyDecision KeyEngine::onKey(int key, bool down) {
    std::lock_guard<std::mutex> lock(mu_);
    KeyDecision d;
    if (!down) {
        auto it = held_.find(key);
        if (it == held_.end()) return d;  // wurde beim Drücken durchgelassen
        const Held h = it.value();
        held_.erase(it);
        d.block = true;
        if (h.kind == Held::Remapped) {
            if (h.to.key) d.out << OutKey{h.to.key, false};
            for (int i = int(h.to.mods.size()) - 1; i >= 0; --i) d.out << OutKey{h.to.mods[i], false};
        } else if (h.kind == Held::MacroKey && h.whileHeld) {
            d.stopMacro = h.macro;
        }
        return d;
    }
    // Tastenwiederholung einer schon umgelegten Taste: nur die Zieltaste wiederholen
    if (auto it = held_.constFind(key); it != held_.constEnd()) {
        d.block = true;
        if (it->kind == Held::Remapped && it->to.key) d.out << OutKey{it->to.key, true};
        return d;
    }
    // Makros vor Umbelegungen: eine Taste mit Makro löst das Makro aus
    for (int i = 0; i < macros_.size(); ++i) {
        const Macro& m = macros_[i];
        if (!m.enabled || !m.trigger || m.steps.isEmpty()) continue;
        if (m.trigger == key || (m.trigger == genericKey(key) && m.trigger != key)) {
            Held h;
            h.kind = Held::MacroKey;
            h.macro = i;
            h.whileHeld = m.mode == Macro::WhileHeld;
            held_.insert(key, h);
            d.block = true;
            d.startMacro = i;
            return d;
        }
    }
    // Genaue Taste vor allgemeiner (RShift vor Shift)
    const Remap* hit = nullptr;
    for (const Remap& r : remaps_)
        if (r.from == key) { hit = &r; break; }
    if (!hit)
        for (const Remap& r : remaps_)
            if (r.from == genericKey(key) && r.from != key) { hit = &r; break; }
    if (!hit) return d;
    Held h;
    h.kind = Held::Remapped;
    h.to = hit->to;
    held_.insert(key, h);
    d.block = true;
    for (int m : hit->to.mods) d.out << OutKey{m, true};
    if (hit->to.key) d.out << OutKey{hit->to.key, true};
    return d;
}

void KeyEngine::releaseAll(QList<OutKey>* out) {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto it = held_.cbegin(); it != held_.cend(); ++it) {
        if (it->kind != Held::Remapped) continue;
        if (it->to.key) *out << OutKey{it->to.key, false};
        for (int i = int(it->to.mods.size()) - 1; i >= 0; --i) *out << OutKey{it->to.mods[i], false};
    }
    held_.clear();
}
