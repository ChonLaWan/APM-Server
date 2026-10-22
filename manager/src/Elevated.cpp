#include "Elevated.h"
#include "Engine.h"
#include "Win.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

QStringList g_log;
void out(const QString& s) { g_log << s; }

int tool(const QString& exe, const QStringList& args)
{
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(exe, args);
    p.waitForFinished(120000);
    for (const QString& l : QString::fromLocal8Bit(p.readAll()).split('\n', Qt::SkipEmptyParts))
        if (!l.trimmed().isEmpty()) out("      " + l.trimmed());
    return p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
}

QString nat(const QString& p) { return QDir::toNativeSeparators(p); }

int installServices(const QString& root, const QString& sid, bool autoStart)
{
    const QString apache = Engine::serviceName(Comp::Apache), maria = Engine::serviceName(Comp::MariaDB);
    if (win::queryService(apache).state == win::SvcState::NotInstalled) {
        out("[..] installing " + apache);
        tool(root + "/apache/bin/httpd.exe", {"-k", "install", "-n", apache, "-d", root + "/apache"});
        if (win::queryService(apache).state == win::SvcState::NotInstalled) { out("[ERR] httpd -k install failed"); return 1; }
    }
    if (win::queryService(maria).state == win::SvcState::NotInstalled) {
        out("[..] installing " + maria);
        tool(root + "/mariadb/bin/mariadbd.exe", {"--install-manual", maria, "--defaults-file=" + nat(root + "/data/mysql/my.ini")});
        if (win::queryService(maria).state == win::SvcState::NotInstalled) { out("[ERR] mariadbd --install failed"); return 1; }
    }
    QString err;
    if (!win::configureService(apache, "APM Apache", "Apache httpd + PHP (FastCGI) of APM at " + nat(root), autoStart, sid, &err) ||
        !win::configureService(maria, "APM MariaDB", "MariaDB server of APM at " + nat(root), autoStart, sid, &err)) {
        out("[ERR] " + err);
        return 1;
    }
    // services run as NT AUTHORITY\LocalService (S-1-5-19): read the tree, write data/logs/tmp
    tool("icacls.exe", {nat(root), "/grant", "*S-1-5-19:(OI)(CI)RX", "/Q"});
    for (const char* d : {"data", "logs", "tmp"})
        tool("icacls.exe", {nat(root + "/" + d), "/grant", "*S-1-5-19:(OI)(CI)M", "/T", "/Q"});
    out(QString("[OK] services installed (LocalService, start: %1)").arg(autoStart ? "automatic" : "manual"));
    return 0;
}

int removeServices()
{
    QString err;
    int rc = 0;
    for (Comp c : {Comp::Apache, Comp::MariaDB}) {
        if (win::deleteService(Engine::serviceName(c), &err)) out("[OK] removed " + Engine::serviceName(c));
        else { out("[ERR] " + err); rc = 1; }
    }
    return rc;
}

int serviceStart(bool autoStart)
{
    QString err;
    for (Comp c : {Comp::Apache, Comp::MariaDB}) {
        if (win::queryService(Engine::serviceName(c)).state == win::SvcState::NotInstalled) continue;
        if (!win::setServiceAutoStart(Engine::serviceName(c), autoStart, &err)) { out("[ERR] " + err); return 1; }
    }
    out(QString("[OK] services start: %1").arg(autoStart ? "automatic" : "manual"));
    return 0;
}

int firewall(const QString& root, bool on, const QString& ports)
{
    const QString name = "APM Apache (LAN)";
    tool("netsh.exe", {"advfirewall", "firewall", "delete", "rule", "name=" + name});
    if (!on) { out("[OK] firewall rule removed"); return 0; }
    const int rc = tool("netsh.exe", {"advfirewall", "firewall", "add", "rule", "name=" + name, "dir=in", "action=allow",
                                      "protocol=TCP", "localport=" + ports, "remoteip=localsubnet", "profile=any",
                                      "program=" + nat(root + "/apache/bin/httpd.exe")});
    out(rc == 0 ? "[OK] firewall: TCP " + ports + " from LocalSubnet -> httpd.exe" : "[ERR] netsh failed");
    return rc == 0 ? 0 : 1;
}

} // namespace

namespace pathops {

namespace {
QStringList apmEntries(const QString& root) { return {nat(root + "/php"), nat(root + "/mariadb/bin")}; }
bool isOurs(const QString& entry, const QStringList& apm)
{
    QString e = entry.trimmed();
    while (e.endsWith('\\')) e.chop(1);
    return e.startsWith("C:\\xampp\\", Qt::CaseInsensitive) || apm.contains(e, Qt::CaseInsensitive);
}
} // namespace

bool contains(bool machine, const QString& root)
{
    const QStringList apm = apmEntries(root);
    const QStringList parts = win::readPath(machine).split(';', Qt::SkipEmptyParts);
    for (const QString& a : apm)
        if (!parts.contains(a, Qt::CaseInsensitive) && !parts.contains(a + "\\", Qt::CaseInsensitive)) return false;
    return true;
}

bool apply(bool machine, const QString& root, bool add, QString* err)
{
    const QStringList apm = apmEntries(root);
    const QStringList parts = win::readPath(machine).split(';', Qt::SkipEmptyParts);
    QStringList kept;
    int at = -1;
    for (const QString& p : parts) {
        if (isOurs(p, apm)) { if (at < 0) at = int(kept.size()); continue; }
        kept << p;
    }
    if (add) {
        if (at < 0) at = int(kept.size());
        for (int i = 0; i < apm.size(); ++i) kept.insert(at + i, apm[i]);
    }
    return win::writePath(machine, kept.join(';'), err);
}

} // namespace pathops

namespace elevated {

bool request(const QStringList& opArgs, QStringList* log)
{
    QTemporaryDir tmp;
    const QString logFile = tmp.filePath("op.log");
    const int rc = win::runElevated(QCoreApplication::applicationFilePath(), QStringList{"--elevated"} + opArgs + QStringList{"--log", logFile});
    QFile f(logFile);
    if (f.open(QIODevice::ReadOnly)) *log << QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts);
    if (rc == -1) *log << "[ERR] cancelled (UAC was declined)";
    return rc == 0;
}

int run(const QStringList& args)
{
    QString logFile;
    const int li = int(args.indexOf("--log"));
    if (li >= 0 && li + 1 < args.size()) logFile = args[li + 1];
    const QString op = args.value(0);
    int rc = 2;
    if (op == "install-services") rc = installServices(args.value(1), args.value(2), args.value(3) == "1");
    else if (op == "remove-services") rc = removeServices();
    else if (op == "service-start") rc = serviceStart(args.value(1) == "1");
    else if (op == "firewall") rc = firewall(args.value(1), args.value(2) == "on", args.value(3));
    else if (op == "path-machine") {
        QString err;
        rc = pathops::apply(true, args.value(1), args.value(2) == "add", &err) ? 0 : 1;
        out(rc == 0 ? "[OK] Machine PATH updated" : "[ERR] " + err);
    } else out("[ERR] unknown op " + op);

    if (!logFile.isEmpty()) {
        QFile f(logFile);
        if (f.open(QIODevice::WriteOnly)) f.write(g_log.join('\n').toUtf8());
    }
    return rc;
}

} // namespace elevated
