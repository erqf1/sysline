#pragma once
#include <QString>
#include <QStringList>

// Deutsch und Englisch. Texte stehen im Code auf Englisch, die deutsche Fassung kommt aus i18n_de.inc.
void setLanguage(const QString& code);  // "de" / "en", leer = Systemsprache
QString language();
QString L(const char* english);
bool hasGerman(const QString& english);  // Selbsttest: fehlt eine Übersetzung?
QStringList missingGerman();  // Texte, die auf Deutsch angefragt wurden, aber keine Übersetzung haben
