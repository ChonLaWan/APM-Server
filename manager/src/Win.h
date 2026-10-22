// Win.h - thin Win32 helpers used by the APM manager (processes, services, elevation, PATH).
#pragma once
#include <QList>
#include <QMultiMap>
#include <QString>
#include <QStringList>

namespace win {

struct Proc {
    quint32 pid = 0;
    quint32 parent = 0;
    QString path;   // full image path, '/' separators
};

// Processes whose exe file name is in `names` (case-insensitive). Processes we may not open are skipped.
QList<Proc> processes(const QStringList& names);
QString processPath(quint32 pid);
bool terminate(quint32 pid);
bool signalEvent(const QString& name);                  // OpenEvent + SetEvent (e.g. "ap1234_shutdown")
bool startHidden(const QString& exe, const QStringList& args, const QString& workDir, quint32* pid = nullptr);

// TCP listeners: port -> owning pid (IPv4 + IPv6)
QMultiMap<quint16, quint32> listeningPorts();

bool isElevated();
QString currentUserSid();
// Runs `exe args` through UAC ("runas") and waits. Returns the exit code, or -1 if refused/failed.
int runElevated(const QString& exe, const QStringList& args);

// --- services -------------------------------------------------------------------------------
enum class SvcState { NotInstalled, Stopped, StartPending, StopPending, Running, Other };
struct SvcInfo {
    SvcState state = SvcState::NotInstalled;
    quint32 pid = 0;
    bool autoStart = false;
};
SvcInfo queryService(const QString& name);
bool startService(const QString& name, QString* err);
bool stopService(const QString& name, QString* err);
// admin only:
bool configureService(const QString& name, const QString& display, const QString& description,
                      bool autoStart, const QString& grantSid, QString* err);
bool setServiceAutoStart(const QString& name, bool autoStart, QString* err);
bool deleteService(const QString& name, QString* err);

// --- PATH (raw registry text, written back as REG_EXPAND_SZ; never expanded) ------------------
QString readPath(bool machine);
bool writePath(bool machine, const QString& value, QString* err);
void broadcastEnvironmentChange();

QString lastErrorText(unsigned long code);

} // namespace win
