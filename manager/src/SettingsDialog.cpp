#include "SettingsDialog.h"
#include "Elevated.h"
#include "Engine.h"
#include "Setup.h"
#include "Win.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QThread>
#include <QVBoxLayout>

namespace {
const char* kRunKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const char* kRunName = "APM Manager";

QLabel* section(const QString& text)
{
    auto* l = new QLabel(text.toUpper());
    l->setObjectName("Section");
    return l;
}

QLabel* note(const QString& text)
{
    auto* l = new QLabel(text);
    l->setObjectName("Small");
    l->setWordWrap(true);
    return l;
}
} // namespace

bool SettingsDialog::runAtSignIn()
{
    return QSettings(kRunKey, QSettings::NativeFormat).contains(kRunName);
}

void SettingsDialog::setRunAtSignIn(bool on)
{
    QSettings s(kRunKey, QSettings::NativeFormat);
    if (on) s.setValue(kRunName, QString("\"%1\" --tray").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));
    else s.remove(kRunName);
}

bool SettingsDialog::startServersOnOpen()
{
    return QSettings("APM", "Manager").value("startServersOnOpen", false).toBool();
}

SettingsDialog::SettingsDialog(Engine* engine, QWidget* parent) : QDialog(parent), m_engine(engine)
{
    setWindowTitle("APM Settings");
    setMinimumWidth(520);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 18, 20, 16);
    v->setSpacing(8);

    // startup
    v->addWidget(section("Startup"));
    m_signIn = new QCheckBox("Open APM Manager when I sign in (in the tray)");
    m_autoServers = new QCheckBox("Start Apache and MariaDB when APM Manager opens");
    v->addWidget(m_signIn);
    v->addWidget(m_autoServers);
    connect(m_signIn, &QCheckBox::toggled, this, [](bool on) { setRunAtSignIn(on); });
    connect(m_autoServers, &QCheckBox::toggled, this, [](bool on) { QSettings("APM", "Manager").setValue("startServersOnOpen", on); });

    // services
    v->addSpacing(8);
    v->addWidget(section("Windows services"));
    v->addWidget(note("Runs Apache and MariaDB as services under the low-privilege LocalService account. "
                      "They can start with Windows before anyone signs in. Installing or removing needs administrator approval."));
    auto* svcRow = new QHBoxLayout;
    m_svcState = new QLabel;
    m_svcInstall = new QPushButton("Install services");
    m_svcRemove = new QPushButton("Remove");
    svcRow->addWidget(m_svcState, 1);
    svcRow->addWidget(m_svcInstall);
    svcRow->addWidget(m_svcRemove);
    v->addLayout(svcRow);
    m_svcBoot = new QCheckBox("Start the services with Windows");
    v->addWidget(m_svcBoot);
    connect(m_svcInstall, &QPushButton::clicked, this, [this] {
        // process-mode servers would hold the ports -> stop first, then hand over to the services
        m_engine->stopAllThen([this](bool) {
            runElevatedOp({"install-services", m_engine->root(), win::currentUserSid(), m_svcBoot->isChecked() ? "1" : "0"},
                          [this](bool ok) { if (ok) m_engine->startAll(); });
        });
    });
    connect(m_svcRemove, &QPushButton::clicked, this, [this] { runElevatedOp({"remove-services"}); });
    connect(m_svcBoot, &QCheckBox::toggled, this, [this](bool on) {
        if (m_updating) return;
        runElevatedOp({"service-start", on ? "1" : "0"});
    });

    // network
    v->addSpacing(8);
    v->addWidget(section("Ports and network"));
    auto* form = new QHBoxLayout;
    auto spin = [](int val) {
        auto* s = new QSpinBox;
        s->setRange(1, 65535);
        s->setValue(val);
        s->setFixedWidth(84);
        s->setButtonSymbols(QAbstractSpinBox::NoButtons);
        return s;
    };
    const ApmConfig cfg = setup::load(m_engine->root());
    m_http = spin(cfg.httpPort);
    m_https = spin(cfg.httpsPort);
    m_db = spin(cfg.dbPort);
    form->addWidget(new QLabel("HTTP"));
    form->addWidget(m_http);
    form->addSpacing(8);
    form->addWidget(new QLabel("HTTPS"));
    form->addWidget(m_https);
    form->addSpacing(8);
    form->addWidget(new QLabel("MariaDB"));
    form->addWidget(m_db);
    form->addStretch();
    v->addLayout(form);
    m_lan = new QCheckBox("Allow other computers on my network to open the sites (opens the firewall for this subnet)");
    m_lan->setChecked(cfg.lan);
    v->addWidget(m_lan);
    v->addWidget(note("MariaDB always stays on this PC only. Applying rewrites the Apache, PHP and MariaDB "
                      "config from the templates and restarts the servers."));
    m_applyNet = new QPushButton("Apply ports and network");
    v->addWidget(m_applyNet, 0, Qt::AlignLeft);
    connect(m_applyNet, &QPushButton::clicked, this, &SettingsDialog::applyNetwork);

    // command line / VS Code
    v->addSpacing(8);
    v->addWidget(section("Command line and VS Code"));
    m_pathState = new QLabel;
    m_pathState->setWordWrap(true);
    v->addWidget(m_pathState);
    auto* pathRow = new QHBoxLayout;
    m_pathAdd = new QPushButton("Add php and mysql to my PATH");
    m_pathRemove = new QPushButton("Remove");
    pathRow->addWidget(m_pathAdd);
    pathRow->addWidget(m_pathRemove);
    pathRow->addStretch();
    v->addLayout(pathRow);
    connect(m_pathAdd, &QPushButton::clicked, this, [this] {
        QString err;
        log({pathops::apply(false, m_engine->root(), true, &err) ? "[OK] added to your PATH - restart VS Code and terminals" : "[ERR] " + err});
        refresh();
    });
    connect(m_pathRemove, &QPushButton::clicked, this, [this] {
        QString err;
        if (pathops::contains(true, m_engine->root()))
            runElevatedOp({"path-machine", m_engine->root(), "remove"});
        log({pathops::apply(false, m_engine->root(), false, &err) ? "[OK] removed from your PATH" : "[ERR] " + err});
        refresh();
    });

    // certificate
    v->addSpacing(8);
    v->addWidget(section("HTTPS certificate"));
    auto* certRow = new QHBoxLayout;
    certRow->addWidget(note("Trust the APM local certificate authority so browsers on this PC accept https://localhost."), 1);
    auto* trust = new QPushButton("Trust certificate");
    certRow->addWidget(trust);
    v->addLayout(certRow);
    connect(trust, &QPushButton::clicked, this, [this] {
        QProcess p;
        p.setProcessChannelMode(QProcess::MergedChannels);
        p.start("certutil.exe", {"-user", "-addstore", "Root", QDir::toNativeSeparators(m_engine->path("ssl/apm-ca.crt"))});
        p.waitForFinished(120000);
        log({p.exitCode() == 0 ? "[OK] APM CA trusted for this user" : "[ERR] certutil: " + QString::fromLocal8Bit(p.readAll()).trimmed()});
    });

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumHeight(110);
    m_log->setPlaceholderText("Messages appear here");
    v->addSpacing(6);
    v->addWidget(m_log);

    auto* close = new QPushButton("Close");
    close->setObjectName("Primary");
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    v->addWidget(close, 0, Qt::AlignRight);

    refresh();
}

void SettingsDialog::refresh()
{
    m_updating = true;
    m_signIn->setChecked(runAtSignIn());
    m_autoServers->setChecked(startServersOnOpen());

    const auto a = win::queryService(Engine::serviceName(Comp::Apache));
    const auto m = win::queryService(Engine::serviceName(Comp::MariaDB));
    const bool installed = a.state != win::SvcState::NotInstalled && m.state != win::SvcState::NotInstalled;
    m_svcState->setText(installed ? "Installed (APM-Apache, APM-MariaDB)" : "Not installed - servers run as normal processes");
    m_svcInstall->setEnabled(!installed);
    m_svcRemove->setEnabled(a.state != win::SvcState::NotInstalled || m.state != win::SvcState::NotInstalled);
    m_svcBoot->setEnabled(installed);
    m_svcBoot->setChecked(installed ? a.autoStart : true);

    const QString phpExe = QStandardPaths::findExecutable("php", win::readPath(true).split(';') + win::readPath(false).split(';'));
    const bool inMachine = pathops::contains(true, m_engine->root());
    const bool inUser = pathops::contains(false, m_engine->root());
    QString where = inMachine && inUser ? "system and user PATH" : inMachine ? "system PATH" : inUser ? "user PATH" : "";
    m_pathState->setText(where.isEmpty()
                             ? QString("APM is not on PATH. `php` in new terminals: %1").arg(phpExe.isEmpty() ? "not found" : QDir::toNativeSeparators(phpExe))
                             : QString("APM php and mysql are on the %1. `php` resolves to %2").arg(where, QDir::toNativeSeparators(phpExe)));
    m_pathAdd->setEnabled(!inUser && !inMachine);
    m_pathRemove->setEnabled(inUser || inMachine);
    m_updating = false;
}

void SettingsDialog::log(const QStringList& lines)
{
    for (const QString& l : lines) m_log->appendPlainText(l);
}

void SettingsDialog::runElevatedOp(const QStringList& op, std::function<void(bool)> after)
{
    setEnabled(false);
    log({"[..] waiting for administrator approval (UAC)"});
    auto* lines = new QStringList;
    auto* ok = new bool(false);
    QThread* th = QThread::create([op, lines, ok] { *ok = elevated::request(op, lines); });
    connect(th, &QThread::finished, this, [this, th, lines, ok, after] {
        log(*lines);
        const bool success = *ok;
        delete lines;
        delete ok;
        th->deleteLater();
        setEnabled(true);
        refresh();
        if (after) after(success);
    });
    th->start();
}

void SettingsDialog::applyNetwork()
{
    ApmConfig cfg = setup::load(m_engine->root());
    const bool lanBefore = cfg.lan;
    cfg.root = m_engine->root();
    cfg.httpPort = m_http->value();
    cfg.httpsPort = m_https->value();
    cfg.dbPort = m_db->value();
    cfg.lan = m_lan->isChecked();
    const bool wasApache = m_engine->status(Comp::Apache).state == CompStatus::State::Running;
    const bool wasMaria = m_engine->status(Comp::MariaDB).state == CompStatus::State::Running;
    setEnabled(false);
    log({"[..] stopping servers"});
    m_engine->stopAllThen([=, this](bool stopped) {
        if (!stopped) { log({"[ERR] servers did not stop - nothing changed"}); setEnabled(true); return; }
        QStringList out;
        const bool ok = setup::run(cfg, true, &out);
        log(out);
        setEnabled(true);
        auto restart = [this, wasApache, wasMaria] {
            if (wasMaria) m_engine->start(Comp::MariaDB);
            if (wasApache) m_engine->start(Comp::Apache);
        };
        if (!ok) { restart(); return; }
        if (cfg.lan || lanBefore) {
            runElevatedOp({"firewall", cfg.root, cfg.lan ? "on" : "off", QString("%1,%2").arg(cfg.httpPort).arg(cfg.httpsPort)},
                          [restart](bool) { restart(); });
        } else {
            restart();
        }
    });
}
