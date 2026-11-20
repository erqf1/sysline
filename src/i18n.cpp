#include "i18n.h"

#include <QHash>
#include <QLocale>
#include <QSet>
#include <mutex>

namespace {
struct Pair {
    const char* en;
    const char* de;
};
const Pair kTable[] = {
#include "i18n_de.inc"
};
QString g_lang = "en";
QSet<QString> g_missing;  // im Selbsttest: deutsche Fassung fehlt

const QHash<QString, QString>& table() {
    static const QHash<QString, QString> h = [] {
        QHash<QString, QString> m;
        for (const Pair& p : kTable) m.insert(QString::fromUtf8(p.en), QString::fromUtf8(p.de));
        return m;
    }();
    return h;
}
}  // namespace

void setLanguage(const QString& code) {
    const QString c = code.isEmpty() ? QLocale::system().name().left(2) : code;
    g_lang = c == "de" ? "de" : "en";
}

QString language() { return g_lang; }

QString L(const char* english) {
    const QString key = QString::fromUtf8(english);
    if (g_lang != "de") return key;
    auto it = table().constFind(key);
    if (it != table().constEnd()) return it.value();
    static std::mutex mu;
    std::lock_guard<std::mutex> lock(mu);
    g_missing.insert(key);
    return key;
}

bool hasGerman(const QString& english) { return table().contains(english); }

QStringList missingGerman() { return QStringList(g_missing.begin(), g_missing.end()); }
