// APM Manager - controls an APM folder (Apache + PHP + MariaDB). See ../README.md.
//   APMManager.exe            open the window
//   APMManager.exe --tray     start in the tray (used by "open at sign-in")
//   APMManager.exe --elevated <op> ... --log <file>   admin helper, launched by the manager itself through UAC
#include "AboutDialog.h"
#include "Elevated.h"
#include "Engine.h"
#include "MainWindow.h"
#include "SettingsDialog.h"
#include "Setup.h"
#include "SetupDialog.h"
#include "Theme.h"
#include "UpdateDialog.h"
#include "Win.h"

#include <QApplication>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QThread>

#include <windows.h>
#include <cstdio>

namespace {

// files replaced by a self-update were renamed to *.old while in use; they can go now
void cleanupOldFiles()
{
    QDirIterator it(QCoreApplication::applicationDirPath(), {"*.old"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) QFile::remove(it.next());
}

// GUI-subsystem exe: keep a redirected stdout (file/pipe) as is, otherwise print into the calling console
void attachStdout()
{
    const HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h && h != INVALID_HANDLE_VALUE && GetFileType(h) != FILE_TYPE_UNKNOWN) return;
    if (AttachConsole(ATTACH_PARENT_PROCESS)) { FILE* f = nullptr; freopen_s(&f, "CONOUT$", "w", stdout); }
}

bool tellRunningInstance(const QString& name)
{
    QLocalSocket s;
    s.connectToServer(name);
    if (!s.waitForConnected(300)) return false;
    s.write("show");
    s.waitForBytesWritten(300);
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == "--elevated") {
            QCoreApplication core(argc, argv);
            return elevated::run(core.arguments().mid(i + 1));
        }
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName("APM Manager");
    QApplication::setOrganizationName("APM");
    QApplication::setQuitOnLastWindowClosed(false);
    const QStringList args = app.arguments();

    // single instance per user: a second launch just brings the window up
    const QString serverName = "APMManager-" + win::currentUserSid();
    auto* server = new QLocalServer(&app);
    const bool restarted = args.contains("--restarted");
    for (int attempt = 0; !server->listen(serverName); ++attempt) {
        if (!restarted && tellRunningInstance(serverName)) return 0;
        if (attempt > 20) { QLocalServer::removeServer(serverName); if (!server->listen(serverName)) break; }
        QThread::msleep(250);   // previous instance (self-update restart) is still shutting down
    }
    cleanupOldFiles();
    theme::apply(app);

    const QString root = Engine::detectRoot();
    if (!QFileInfo::exists(root + "/apache/bin/httpd.exe")) {
        QMessageBox::critical(nullptr, "APM",
                              QString("APM engines were not found.\n\nExpected %1\\apache\\bin\\httpd.exe.\n"
                                      "Keep APMManager.exe in the 'manager' folder of an unpacked APM package.")
                                  .arg(QDir::toNativeSeparators(root)));
        return 1;
    }
    // headless: APMManager.exe --setup [--http N] [--https N] [--db N] [--lan] [--reset]
    if (args.contains("--setup")) {
        attachStdout();
        ApmConfig c = QFileInfo::exists(root + "/apache/conf/httpd.conf") ? setup::load(root) : setup::suggest(root);
        c.root = root;
        auto opt = [&args](const QString& name, int def) {
            const int i = int(args.indexOf(name));
            return i >= 0 && i + 1 < args.size() ? args[i + 1].toInt() : def;
        };
        c.httpPort = opt("--http", c.httpPort);
        c.httpsPort = opt("--https", c.httpsPort);
        c.dbPort = opt("--db", c.dbPort);
        if (args.contains("--lan")) c.lan = true;
        QStringList out;
        const bool ok = setup::run(c, args.contains("--reset"), &out);
        printf("%s\n", out.join('\n').toLocal8Bit().constData());
        fflush(stdout);
        return ok ? 0 : 1;
    }
    // console control, same engine as the window:
    //   APMManager.exe --cli status|start|stop|restart [apache|mariadb|all]
    const int cli = int(args.indexOf("--cli"));
    if (cli >= 0) {
        attachStdout();
        const QString cmd = args.value(cli + 1, "status");
        const QString which = args.value(cli + 2, "all");
        if (!setup::isReady(root)) {
            printf("APM in %s is not set up for this folder%s. Run: APMManager.exe --setup\n",
                   qPrintable(QDir::toNativeSeparators(root)), setup::wasMoved(root) ? " (it was moved or copied)" : "");
            fflush(stdout);
            return 2;
        }
        auto* e = new Engine(root, &app);
        auto print = [e] {
            for (Comp c : {Comp::Apache, Comp::MariaDB}) {
                const CompStatus& s = e->status(c);
                static const char* names[] = {"missing", "stopped", "starting", "running", "stopping"};
                QStringList ports;
                for (quint16 p : s.ports) ports << QString::number(p);
                printf("%-8s %-9s %-8s ports %-8s %s%s\n", qPrintable(e->displayName(c)), names[int(s.state)],
                       qPrintable(s.version), qPrintable(ports.join(',')), s.service ? "service" : "process",
                       s.message.isEmpty() ? "" : qPrintable("  ! " + s.message));
            }
            printf("PHP      %-9s %-8s workers %d\n", "", qPrintable(e->phpVersion()), e->phpWorkers());
            fflush(stdout);
        };
        QTimer::singleShot(1200, e, [e, cmd, which, print] {
            QList<Comp> comps;
            if (which != "mariadb") comps << Comp::Apache;
            if (which != "apache") comps << Comp::MariaDB;
            for (Comp c : comps) {
                if (cmd == "start") e->start(c);
                else if (cmd == "stop") e->stop(c);
                else if (cmd == "restart") e->restart(c);
            }
            auto* clock = new QElapsedTimer;
            clock->start();
            auto* t = new QTimer(e);
            QObject::connect(t, &QTimer::timeout, e, [e, t, clock, print] {
                if (e->busy() && clock->elapsed() < 90000) return;
                t->stop();
                print();
                bool failed = false;
                for (Comp c : {Comp::Apache, Comp::MariaDB}) failed |= !e->status(c).message.isEmpty();
                QCoreApplication::exit(failed ? 1 : 0);
            });
            t->start(cmd == "status" ? 10 : 400);
        });
        return app.exec();
    }

    if (!setup::isReady(root)) {
        SetupDialog d(root);
        if (d.exec() != QDialog::Accepted) return 0;
    }

    Engine engine(root);
    MainWindow w(&engine);
    QObject::connect(server, &QLocalServer::newConnection, &w, [server, &w] {
        while (QLocalSocket* s = server->nextPendingConnection()) {
            s->waitForReadyRead(300);
            s->deleteLater();
            w.showNormalAndRaise();
        }
    });

    if (args.contains("--tray")) w.startHidden();
    else w.show();

    // --screenshot <png> [--page main|settings|update]: render a window to a file and exit (docs, CI smoke test)
    const int shot = int(args.indexOf("--screenshot"));
    if (shot >= 0 && shot + 1 < args.size()) {
        const QString file = args[shot + 1];
        const int pi = int(args.indexOf("--page"));
        const QString page = pi >= 0 ? args.value(pi + 1) : "main";
        QTimer::singleShot(2500, &w, [&w, &engine, file, page] {
            QWidget* target = &w;
            QDialog* dlg = nullptr;
            if (page == "settings") dlg = new SettingsDialog(&engine, &w);
            else if (page == "update") dlg = new UpdateDialog(&engine, &w);
            else if (page == "about") dlg = new AboutDialog(&engine, &w);
            if (dlg) { dlg->show(); dlg->adjustSize(); QCoreApplication::processEvents(); target = dlg; }
            QTimer::singleShot(400, [target, file] {
                target->grab().save(file);
                QCoreApplication::exit(0);
            });
        });
    }

    if (SettingsDialog::startServersOnOpen())
        QTimer::singleShot(800, &engine, [&engine] {
            if (!engine.anyRunning()) engine.startAll();
        });
    return app.exec();
}
