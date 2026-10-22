// Setup.h - first-run / relocation setup of an APM folder: renders config from <root>\templates,
// issues the local CA + server cert, creates the MariaDB datadir and records the result in apm.json.
// This is what makes the zip package usable on any machine and in any folder.
#pragma once
#include <QString>
#include <QStringList>

struct ApmConfig {
    QString root;              // forward slashes
    int httpPort = 80;
    int httpsPort = 443;
    int dbPort = 3306;
    bool lan = false;
};

namespace setup {
// apm.json present and written for this exact folder?
bool isReady(const QString& root);
// folder was moved/copied since setup (config files point elsewhere)
bool wasMoved(const QString& root);
ApmConfig load(const QString& root);
// suggest free ports (80/443/3306, else 8080/8443/3307, else +1...)
ApmConfig suggest(const QString& root);
QString busyPortOwner(int port);   // "" if free
// render + cert + datadir. `resetConfig` overwrites existing config files. Appends progress lines to *log.
bool run(const ApmConfig& cfg, bool resetConfig, QStringList* log);
} // namespace setup
