// Engine.h - knows the APM layout and drives Apache + MariaDB, either as Windows services (if installed)
// or as plain background processes. Nothing is hard-coded to a version: versions are read from the
// executables (re-probed whenever an exe changes on disk), so updating the engines keeps working.
#pragma once
#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <functional>

enum class Comp { Apache, MariaDB };

struct CompStatus {
    enum class State { Missing, Stopped, Starting, Running, Stopping };
    State state = State::Stopped;
    bool service = false;        // installed as a Windows service
    bool serviceAuto = false;    // service start type = automatic
    QList<quint32> pids;
    QString version;
    QList<quint16> ports;
    QString message;             // last error / conflict, shown under the card
};

class Engine : public QObject {
    Q_OBJECT
public:
    explicit Engine(const QString& root, QObject* parent = nullptr);

    static QString detectRoot();
    QString root() const { return m_root; }
    QString path(const QString& rel) const;

    const CompStatus& status(Comp c) const { return m_st[int(c)]; }
    QString phpVersion() const { return m_phpVersion; }
    int phpWorkers() const { return m_phpWorkers; }
    quint16 httpPort() const { return m_httpPort; }
    quint16 httpsPort() const { return m_httpsPort; }

    static QString serviceName(Comp c) { return c == Comp::Apache ? "APM-Apache" : "APM-MariaDB"; }
    QString displayName(Comp c) const { return c == Comp::Apache ? "Apache" : "MariaDB"; }
    QString exePath(Comp c) const;
    QString logPath(Comp c) const;
    QString configPath(Comp c) const;
    QString phpIni() const { return path("php/php.ini"); }
    QString phpLog() const { return path("logs/php/php_error.log"); }

    bool anyRunning() const;
    bool allRunning() const;
    bool busy() const;

public slots:
    void refresh();
    void start(Comp c);
    void stop(Comp c);
    void restart(Comp c);
    void startAll();
    void stopAll();
    // stop everything, then call back with true once no APM process is left (false on timeout)
    void stopAllThen(std::function<void(bool)> done, int timeoutMs = 60000);

signals:
    void changed();
    void notify(const QString& title, const QString& text);

private:
    struct Pending {
        enum Kind { None, Start, Stop } kind = None;
        QElapsedTimer since;
        int timeoutMs = 0;
        quint32 startedPid = 0;
        bool forced = false;
        bool restartAfter = false;
    };

    void probeVersion(Comp c);
    void probePhp();
    void readPorts();
    QString portConflict(Comp c, const QMultiMap<quint16, quint32>& listening) const;
    QString logTail(Comp c) const;
    void finishStop(Comp c);
    void forceKill(Comp c);

    QString m_root;
    CompStatus m_st[2];
    Pending m_pending[2];
    QString m_phpVersion;
    int m_phpWorkers = 0;
    quint16 m_httpPort = 80;
    quint16 m_httpsPort = 443;
    QHash<QString, QDateTime> m_probed;     // exe -> mtime of the last version probe
    QHash<QString, QDateTime> m_confSeen;   // config file -> mtime of the last port parse
    QTimer m_timer;
};
