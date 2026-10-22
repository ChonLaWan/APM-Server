#include "Setup.h"
#include "Win.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QProcess>
#include <QRegularExpression>

namespace {

QString jsonPath(const QString& root) { return root + "/apm.json"; }

QString readText(const QString& f)
{
    QFile file(f);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

bool writeText(const QString& f, const QString& text)
{
    QDir().mkpath(QFileInfo(f).absolutePath());
    QFile file(f);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    file.write(text.toUtf8());
    return true;
}

bool render(const ApmConfig& c, const QString& tpl, const QString& dest, bool winPaths, bool reset, QStringList* log)
{
    if (QFileInfo::exists(dest) && !reset) {
        *log << "[KEEP] " + QDir::toNativeSeparators(dest);
        return true;
    }
    QString t = readText(c.root + "/templates/" + tpl);
    if (t.isEmpty()) {
        *log << "[ERR] template missing: templates\\" + tpl;
        return false;
    }
    t.replace("{{APM_ROOT}}", winPaths ? QDir::toNativeSeparators(c.root) : c.root);
    t.replace("{{HTTP_PORT}}", QString::number(c.httpPort));
    t.replace("{{HTTPS_PORT}}", QString::number(c.httpsPort));
    t.replace("{{DB_PORT}}", QString::number(c.dbPort));
    t.replace("{{BIND}}", c.lan ? "0.0.0.0" : "127.0.0.1");
    if (!writeText(dest, t)) {
        *log << "[ERR] cannot write " + QDir::toNativeSeparators(dest);
        return false;
    }
    *log << "[OK] " + QDir::toNativeSeparators(dest);
    return true;
}

int runTool(const QString& exe, const QStringList& args, QStringList* log)
{
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(exe, args);
    if (!p.waitForFinished(180000)) {
        *log << "[ERR] timeout: " + QFileInfo(exe).fileName();
        p.kill();
        return -1;
    }
    for (const QString& l : QString::fromLocal8Bit(p.readAll()).split('\n', Qt::SkipEmptyParts))
        if (!l.trimmed().isEmpty()) *log << "      " + l.trimmed();
    return p.exitCode();
}

QStringList lanAddresses()
{
    QStringList out;
    for (const QHostAddress& a : QNetworkInterface::allAddresses())
        if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback() && !a.isLinkLocal()) out << a.toString();
    return out;
}

} // namespace

namespace setup {

bool isReady(const QString& root)
{
    const auto o = QJsonDocument::fromJson(readText(jsonPath(root)).toUtf8()).object();
    return !o.isEmpty() && o.value("root").toString().compare(root, Qt::CaseInsensitive) == 0 &&
           QFileInfo::exists(root + "/apache/conf/httpd.conf") && QFileInfo::exists(root + "/data/mysql/mysql");
}

bool wasMoved(const QString& root)
{
    const auto o = QJsonDocument::fromJson(readText(jsonPath(root)).toUtf8()).object();
    if (!o.isEmpty()) return o.value("root").toString().compare(root, Qt::CaseInsensitive) != 0;
    // no apm.json (deployed by an older script): look at the Define in httpd.conf
    static const QRegularExpression def(R"re(Define\s+APM\s+"([^"]+)")re");
    const auto m = def.match(readText(root + "/apache/conf/httpd.conf"));
    return m.hasMatch() && m.captured(1).compare(root, Qt::CaseInsensitive) != 0;
}

ApmConfig load(const QString& root)
{
    ApmConfig c;
    c.root = root;
    const auto o = QJsonDocument::fromJson(readText(jsonPath(root)).toUtf8()).object();
    if (!o.isEmpty()) {
        c.httpPort = o.value("httpPort").toInt(80);
        c.httpsPort = o.value("httpsPort").toInt(443);
        c.dbPort = o.value("dbPort").toInt(3306);
        c.lan = o.value("lan").toBool();
        return c;
    }
    // infer from existing config (folder deployed before apm.json existed)
    static const QRegularExpression listen(R"(^\s*Listen\s+(\S*:)?(\d+))", QRegularExpression::MultilineOption);
    static const QRegularExpression port(R"(^\s*port\s*=\s*(\d+))", QRegularExpression::MultilineOption);
    auto m = listen.match(readText(root + "/apache/conf/httpd.conf"));
    if (m.hasMatch()) { c.httpPort = m.captured(2).toInt(); c.lan = m.captured(1).startsWith("0.0.0.0"); }
    m = listen.match(readText(root + "/apache/conf/extra/apm-ssl.conf"));
    if (m.hasMatch()) c.httpsPort = m.captured(2).toInt();
    m = port.match(readText(root + "/data/mysql/my.ini"));
    if (m.hasMatch()) c.dbPort = m.captured(1).toInt();
    return c;
}

QString busyPortOwner(int port)
{
    const auto l = win::listeningPorts();
    if (!l.contains(quint16(port))) return {};
    const QString p = win::processPath(l.value(quint16(port)));
    return p.isEmpty() ? QString("another program") : QFileInfo(p).fileName();
}

ApmConfig suggest(const QString& root)
{
    ApmConfig c;
    c.root = root;
    auto pick = [](std::initializer_list<int> prefs) {
        for (int p : prefs)
            if (busyPortOwner(p).isEmpty()) return p;
        int p = *(prefs.end() - 1);
        while (!busyPortOwner(++p).isEmpty()) {}
        return p;
    };
    c.httpPort = pick({80, 8080});
    c.httpsPort = pick({443, 8443});
    c.dbPort = pick({3306, 3307});
    return c;
}

bool run(const ApmConfig& c, bool resetConfig, QStringList* log)
{
    const QString r = c.root;
    for (const char* d : {"www/default", "logs/apache", "logs/php", "logs/mariadb", "tmp/sessions", "tmp/uploads", "ssl", "data"})
        QDir().mkpath(r + "/" + d);

    const bool moved = wasMoved(r);
    const bool reset = resetConfig || moved;
    if (moved) *log << "[..] folder moved/copied since last setup - re-rendering config for " + QDir::toNativeSeparators(r);

    bool ok = render(c, "httpd.conf", r + "/apache/conf/httpd.conf", false, reset, log) &&
              render(c, "apm-ssl.conf", r + "/apache/conf/extra/apm-ssl.conf", false, reset, log) &&
              render(c, "php.ini", r + "/php/php.ini", true, reset, log) &&
              render(c, "my.ini", r + "/mariadb/my.ini", false, reset, log);
    if (!ok) return false;
    QDir().mkpath(r + "/apache/conf/vhosts");
    if (!QFileInfo::exists(r + "/www/default/index.php") && QFileInfo::exists(r + "/templates/index.php"))
        QFile::copy(r + "/templates/index.php", r + "/www/default/index.php");

    // certificates (CA kept forever; server cert re-issued on reset so the SAN matches)
    if (!QFileInfo::exists(r + "/ssl/localhost.crt") || reset) {
        QStringList hosts = {"localhost", "127.0.0.1", "::1"};
        if (c.lan) hosts << QHostInfo::localHostName().toLower() << lanAddresses();
        const QString php = r + "/php/php.exe";
        // value quoted: ( ) and other characters in a folder name are INI syntax for php -d
        if (runTool(php, QStringList{"-n", "-d", "extension_dir=\"" + QDir::toNativeSeparators(r + "/php/ext") + "\"", "-d",
                                     "extension=openssl", QDir::toNativeSeparators(r + "/tools/mkcert.php"),
                                     QDir::toNativeSeparators(r + "/ssl")} + hosts, log) != 0) {
            *log << "[ERR] certificate generation failed";
            return false;
        }
    }
    if (!QFileInfo::exists(r + "/ssl/cacert.pem")) *log << "[WARN] ssl\\cacert.pem missing - PHP curl/openssl cannot verify HTTPS";

    // MariaDB datadir: create once; afterwards the server reads <datadir>\my.ini -> keep it in sync
    const QString myini = QDir::toNativeSeparators(r + "/mariadb/my.ini");
    if (!QFileInfo::exists(r + "/data/mysql/mysql")) {
        *log << "[..] creating MariaDB datadir";
        if (runTool(r + "/mariadb/bin/mariadb-install-db.exe",
                    {"--datadir=" + QDir::toNativeSeparators(r + "/data/mysql"), "--port=" + QString::number(c.dbPort),
                     "--config=" + myini, "--silent"}, log) != 0) {
            *log << "[ERR] mariadb-install-db failed";
            return false;
        }
        *log << "[OK] datadir (root@localhost, no password)";
    } else if (reset) {
        QFile::remove(r + "/data/mysql/my.ini");
        QFile::copy(r + "/mariadb/my.ini", r + "/data/mysql/my.ini");
        *log << "[OK] data\\mysql\\my.ini synced";
    }

    // config check (parses only)
    if (runTool(r + "/apache/bin/httpd.exe", {"-t", "-d", r + "/apache"}, log) != 0) {
        *log << "[ERR] httpd -t failed";
        return false;
    }

    QJsonObject o{{"root", r}, {"httpPort", c.httpPort}, {"httpsPort", c.httpsPort}, {"dbPort", c.dbPort}, {"lan", c.lan},
                  {"setupAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
    writeText(jsonPath(r), QString::fromUtf8(QJsonDocument(o).toJson()));
    *log << QString("[OK] ready: http %1, https %2, db 127.0.0.1:%3%4")
                .arg(c.httpPort).arg(c.httpsPort).arg(c.dbPort).arg(c.lan ? ", LAN on" : "");
    return true;
}

} // namespace setup
