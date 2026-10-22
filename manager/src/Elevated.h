// Elevated.h - admin-only operations. The GUI never runs elevated; it re-launches itself through UAC
// as `APMManager.exe --elevated <op> ... --log <file>` and reads the log back.
#pragma once
#include <QString>
#include <QStringList>

namespace elevated {
// GUI side: run an op through UAC. Returns true on success; *log receives the op's output lines.
bool request(const QStringList& opArgs, QStringList* log);
// elevated side: dispatch argv (after "--elevated"). Returns the process exit code.
int run(const QStringList& args);
} // namespace elevated

namespace pathops {
// Add/remove <root>\php and <root>\mariadb\bin in the Machine or User PATH. Dead C:\xampp\ entries are
// dropped; the APM entries take the place of the first removed entry. Raw registry, REG_EXPAND_SZ.
bool apply(bool machine, const QString& root, bool add, QString* err);
bool contains(bool machine, const QString& root);
} // namespace pathops
