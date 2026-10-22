#include "Engine.h"
#include "Win.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

namespace {
const QStringList kProcNames = {"httpd.exe", "mariadbd.exe", "php-cgi.exe"};

bool under(const QString& path, const QString& dir)
{
    return path.startsWith(dir + "/", Qt::CaseInsensitive);
}

QString readText(const QString& file)
{
    QFile f(file);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}
} // namespace

Engine::Engine(const QString& root, QObject* parent)
    : QObject(parent), m_root(QDir::cleanPath(QDir::fromNativeSeparators(root)))
{
    m_timer.setInterval(1500);
    connect(&m_timer, &QTimer::timeout, this, &Engine::refresh);
    m_timer.start();
    QTimer::singleShot(0, this, &Engine::refresh);
}

QString Engine::detectRoot()
{
    // 1. explicit override, 2. the folder this manager lives in (<root>\manager\APMManager.exe), 3. C:\APM
    const QString env = qEnvironmentVariable("APM_ROOT");
    if (!env.isEmpty() && QFileInfo::exists(env + "/apache")) return QDir::cleanPath(QDir::fromNativeSeparators(env));
    QDir d(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 2; ++i) {
        if (QFileInfo::exists(d.filePath("apache/bin/httpd.exe"))) return d.absolutePath();
        if (!d.cdUp()) break;
    }
    return "C:/APM";
}

QString Engine::path(const QString& rel) const { return m_root + "/" + rel; }

QString Engine::exePath(Comp c) const
{
    return path(c == Comp::Apache ? "apache/bin/httpd.exe" : "mariadb/bin/mariadbd.exe");
}

QString Engine::logPath(Comp c) const
{
    return path(c == Comp::Apache ? "logs/apache/error.log" : "logs/mariadb/mariadb.err");
}

QString Engine::configPath(Comp c) const
{
    return path(c == Comp::Apache ? "apache/conf/httpd.conf" : "data/mysql/my.ini");
}

bool Engine::anyRunning() const
{
    for (const auto& s : m_st)
        if (s.state == CompStatus::State::Running || s.state == CompStatus::State::Starting) return true;
    return false;
}

bool Engine::allRunning() const
{
    for (const auto& s : m_st)
        if (s.state != CompStatus::State::Running) return false;
    return true;
}

bool Engine::busy() const
{
    for (const auto& p : m_pending)
        if (p.kind != Pending::None) return true;
    return false;
}

// --- polling -------------------------------------------------------------------------------------

void Engine::refresh()
{
    readPorts();
    const QList<win::Proc> procs = win::processes(kProcNames);
    const QMultiMap<quint16, quint32> listening = win::listeningPorts();

    for (Comp c : {Comp::Apache, Comp::MariaDB}) {
        CompStatus& s = m_st[int(c)];
        Pending& p = m_pending[int(c)];
        const QString exe = exePath(c);

        if (!QFileInfo::exists(exe)) {
            s.state = CompStatus::State::Missing;
            s.pids.clear();
            continue;
        }
        probeVersion(c);

        const win::SvcInfo svc = win::queryService(serviceName(c));
        s.service = svc.state != win::SvcState::NotInstalled;
        s.serviceAuto = svc.autoStart;

        s.pids.clear();
        for (const auto& pr : procs)
            if (pr.path.compare(exe, Qt::CaseInsensitive) == 0) s.pids << pr.pid;
        if (s.service && svc.pid && !s.pids.contains(svc.pid)) s.pids << svc.pid;

        bool running = !s.pids.isEmpty();
        if (s.service) {
            running = svc.state == win::SvcState::Running;
            if (svc.state == win::SvcState::StartPending) s.state = CompStatus::State::Starting;
            else if (svc.state == win::SvcState::StopPending) s.state = CompStatus::State::Stopping;
        }

        switch (p.kind) {
        case Pending::Start:
            if (running) {
                // a process-mode Apache that dies right after launch still shows up for a moment
                if (!s.service && p.since.elapsed() < 2500) { s.state = CompStatus::State::Starting; break; }
                p.kind = Pending::None;
                s.state = CompStatus::State::Running;
                s.message.clear();
            } else if ((!s.service && p.startedPid && p.since.elapsed() > 1500) || p.since.elapsed() > p.timeoutMs) {
                p.kind = Pending::None;
                s.state = CompStatus::State::Stopped;
                s.message = "Did not start. " + logTail(c);
                emit notify(displayName(c) + " did not start", s.message);
            } else {
                s.state = CompStatus::State::Starting;
            }
            break;
        case Pending::Stop:
            if (!running && (c != Comp::Apache || std::none_of(procs.begin(), procs.end(), [&](const win::Proc& pr) {
                                   return under(pr.path, m_root);
                               }))) {
                finishStop(c);
            } else if (p.since.elapsed() > p.timeoutMs && !s.service && !p.forced) {
                forceKill(c);
                s.state = CompStatus::State::Stopping;
            } else if (p.since.elapsed() > p.timeoutMs + 15000) {
                p.kind = Pending::None;
                s.state = running ? CompStatus::State::Running : CompStatus::State::Stopped;
                s.message = "Did not stop in time.";
            } else {
                s.state = CompStatus::State::Stopping;
            }
            break;
        case Pending::None:
            if (!s.service || (svc.state != win::SvcState::StartPending && svc.state != win::SvcState::StopPending))
                s.state = running ? CompStatus::State::Running : CompStatus::State::Stopped;
            if (s.state == CompStatus::State::Stopped) {
                const QString conflict = portConflict(c, listening);
                if (!conflict.isEmpty()) s.message = conflict;
                else if (s.message.startsWith("Port ")) s.message.clear();
            }
            break;
        }
    }

    m_phpWorkers = 0;
    for (const auto& pr : procs)
        if (pr.path.compare(path("php/php-cgi.exe"), Qt::CaseInsensitive) == 0) ++m_phpWorkers;
    probePhp();
    emit changed();
}

void Engine::readPorts()
{
    auto changedSince = [this](const QString& f) {
        const QDateTime m = QFileInfo(f).lastModified();
        if (m_confSeen.value(f) == m) return false;
        m_confSeen[f] = m;
        return true;
    };
    const QString httpd = configPath(Comp::Apache), ssl = path("apache/conf/extra/apm-ssl.conf"), myini = configPath(Comp::MariaDB);
    if (changedSince(httpd) | changedSince(ssl)) {
        static const QRegularExpression listen(R"(^\s*Listen\s+(?:\S*:)?(\d+))", QRegularExpression::MultilineOption);
        QList<quint16> ports;
        auto http = listen.globalMatch(readText(httpd));
        if (http.hasNext()) ports << (m_httpPort = quint16(http.next().captured(1).toUInt()));
        auto https = listen.globalMatch(readText(ssl));
        if (https.hasNext()) ports << (m_httpsPort = quint16(https.next().captured(1).toUInt()));
        m_st[int(Comp::Apache)].ports = ports;
    }
    if (changedSince(myini)) {
        static const QRegularExpression port(R"(^\s*port\s*=\s*(\d+))", QRegularExpression::MultilineOption);
        const auto m = port.match(readText(myini));
        m_st[int(Comp::MariaDB)].ports = m.hasMatch() ? QList<quint16>{quint16(m.captured(1).toUInt())} : QList<quint16>{};
    }
}

QString Engine::portConflict(Comp c, const QMultiMap<quint16, quint32>& listening) const
{
    for (quint16 port : m_st[int(c)].ports) {
        for (quint32 pid : listening.values(port)) {
            const QString owner = win::processPath(pid);
            if (!owner.isEmpty() && under(owner, m_root)) continue;
            const QString name = owner.isEmpty() ? QString("pid %1").arg(pid) : QFileInfo(owner).fileName();
            return QString("Port %1 is in use by %2").arg(port).arg(name);
        }
    }
    return {};
}

void Engine::probeVersion(Comp c)
{
    const QString exe = exePath(c);
    const QDateTime m = QFileInfo(exe).lastModified();
    if (m_probed.value(exe) == m) return;
    m_probed[exe] = m;
    auto* pr = new QProcess(this);
    connect(pr, &QProcess::finished, this, [this, pr, c] {
        const QString out = QString::fromLocal8Bit(pr->readAllStandardOutput() + pr->readAllStandardError());
        static const QRegularExpression apache(R"(Apache/([\d.]+))"), maria(R"(Ver\s+([\d.]+)-MariaDB)");
        const auto mm = (c == Comp::Apache ? apache : maria).match(out);
        m_st[int(c)].version = mm.hasMatch() ? mm.captured(1) : QString();
        pr->deleteLater();
        emit changed();
    });
    pr->start(exe, c == Comp::Apache ? QStringList{"-v"} : QStringList{"--version"});
}

void Engine::probePhp()
{
    const QString exe = path("php/php-cgi.exe");
    const QDateTime m = QFileInfo(exe).lastModified();
    if (!m.isValid() || m_probed.value(exe) == m) return;
    m_probed[exe] = m;
    auto* pr = new QProcess(this);
    connect(pr, &QProcess::finished, this, [this, pr] {
        static const QRegularExpression re(R"(PHP\s+([\d.]+))");
        const auto mm = re.match(QString::fromLocal8Bit(pr->readAllStandardOutput()));
        m_phpVersion = mm.hasMatch() ? mm.captured(1) : QString();
        pr->deleteLater();
        emit changed();
    });
    pr->start(exe, {"-n", "-v"});
}

QString Engine::logTail(Comp c) const
{
    QFile f(logPath(c));
    if (!f.open(QIODevice::ReadOnly)) return "See the log.";
    if (f.size() > 16384) f.seek(f.size() - 16384);
    const QStringList lines = QString::fromLocal8Bit(f.readAll()).split('\n', Qt::SkipEmptyParts);
    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
        if (it->contains("error", Qt::CaseInsensitive) || it->contains("emerg") || it->contains("AH00")) {
            QString l = it->trimmed();
            static const QRegularExpression stamp(R"(^\[[^\]]*\]\s*(\[[^\]]*\]\s*)*)");
            l.remove(stamp);
            return l.left(220);
        }
    }
    return "See the log.";
}

// --- actions ------------------------------------------------------------------------------------

void Engine::start(Comp c)
{
    CompStatus& s = m_st[int(c)];
    Pending& p = m_pending[int(c)];
    if (s.state == CompStatus::State::Running || s.state == CompStatus::State::Missing || p.kind != Pending::None) return;
    s.message.clear();

    if (!s.service) {
        const QString conflict = portConflict(c, win::listeningPorts());
        if (!conflict.isEmpty()) {
            s.message = conflict;
            emit notify(displayName(c) + " not started", conflict);
            emit changed();
            return;
        }
    }

    p = Pending{};
    p.kind = Pending::Start;
    p.since.start();
    p.timeoutMs = 30000;
    bool ok = true;
    QString err;
    if (s.service) {
        ok = win::startService(serviceName(c), &err);
    } else if (c == Comp::Apache) {
        QFile::remove(path("logs/apache/httpd.pid"));   // stale pid file from an unclean stop
        ok = win::startHidden(exePath(c), {"-d", path("apache")}, path("apache"), &p.startedPid);
    } else {
        ok = win::startHidden(exePath(c), {"--defaults-file=" + QDir::toNativeSeparators(configPath(c))},
                              path("mariadb"), &p.startedPid);
    }
    if (!ok) {
        p.kind = Pending::None;
        s.message = err.isEmpty() ? "Could not launch " + QFileInfo(exePath(c)).fileName() : err;
        emit notify(displayName(c) + " not started", s.message);
    } else {
        s.state = CompStatus::State::Starting;
    }
    emit changed();
}

void Engine::stop(Comp c)
{
    CompStatus& s = m_st[int(c)];
    Pending& p = m_pending[int(c)];
    if (s.state == CompStatus::State::Stopped || s.state == CompStatus::State::Missing) return;
    const bool restartAfter = p.restartAfter;
    p = Pending{};
    p.kind = Pending::Stop;
    p.since.start();
    p.restartAfter = restartAfter;
    p.timeoutMs = c == Comp::Apache ? 10000 : 30000;
    s.message.clear();

    if (s.service) {
        QString err;
        if (!win::stopService(serviceName(c), &err)) { s.message = err; p.kind = Pending::None; }
    } else if (c == Comp::Apache) {
        // graceful: mpm_winnt's parent waits on the named event "ap<parent pid>_shutdown"
        bool signalled = false;
        QFile pidf(path("logs/apache/httpd.pid"));
        if (pidf.open(QIODevice::ReadOnly)) signalled = win::signalEvent(QString("ap%1_shutdown").arg(pidf.readAll().trimmed().toUInt()));
        if (!signalled) forceKill(c);
    } else {
        // graceful: mariadbd (console mode) waits on "MySQLShutdown<pid>"; fall back to mariadb-admin
        bool signalled = false;
        for (quint32 pid : s.pids) signalled |= win::signalEvent(QString("MySQLShutdown%1").arg(pid));
        if (!signalled) {
            const QString port = s.ports.isEmpty() ? "3306" : QString::number(s.ports.first());
            QProcess::startDetached(path("mariadb/bin/mariadb-admin.exe"),
                                    {"-uroot", "--port=" + port, "--protocol=tcp", "shutdown"});
        }
    }
    s.state = CompStatus::State::Stopping;
    emit changed();
}

void Engine::restart(Comp c)
{
    const CompStatus& s = m_st[int(c)];
    if (c == Comp::Apache && !s.service && s.state == CompStatus::State::Running) {
        // graceful restart re-reads the config without dropping the listener
        QFile pidf(path("logs/apache/httpd.pid"));
        if (pidf.open(QIODevice::ReadOnly) && win::signalEvent(QString("ap%1_restart").arg(pidf.readAll().trimmed().toUInt()))) {
            emit notify("Apache", "Restarting (config reloaded)");
            return;
        }
    }
    if (s.state == CompStatus::State::Stopped) { start(c); return; }
    m_pending[int(c)].restartAfter = true;
    stop(c);
}

void Engine::finishStop(Comp c)
{
    Pending& p = m_pending[int(c)];
    const bool again = p.restartAfter;
    p = Pending{};
    m_st[int(c)].state = CompStatus::State::Stopped;
    if (c == Comp::Apache) QFile::remove(path("logs/apache/httpd.pid"));
    if (again) QTimer::singleShot(300, this, [this, c] { start(c); });
}

void Engine::forceKill(Comp c)
{
    m_pending[int(c)].forced = true;
    const QString exe = exePath(c);
    for (const auto& pr : win::processes(kProcNames)) {
        const bool mine = pr.path.compare(exe, Qt::CaseInsensitive) == 0 ||
                          (c == Comp::Apache && pr.path.compare(path("php/php-cgi.exe"), Qt::CaseInsensitive) == 0);
        if (mine) win::terminate(pr.pid);
    }
}

void Engine::startAll()
{
    start(Comp::MariaDB);
    start(Comp::Apache);
}

void Engine::stopAll()
{
    stop(Comp::Apache);
    stop(Comp::MariaDB);
}

void Engine::stopAllThen(std::function<void(bool)> done, int timeoutMs)
{
    stopAll();
    auto* t = new QTimer(this);
    auto* clock = new QElapsedTimer;
    clock->start();
    connect(t, &QTimer::timeout, this, [this, t, clock, done, timeoutMs] {
        refresh();
        const bool left = std::any_of(m_st, m_st + 2, [](const CompStatus& s) {
            return s.state != CompStatus::State::Stopped && s.state != CompStatus::State::Missing;
        }) || m_phpWorkers > 0;
        if (!left || clock->elapsed() > timeoutMs) {
            t->stop();
            t->deleteLater();
            delete clock;
            done(!left);
        }
    });
    t->start(500);
}
