#include "MainWindow.h"
#include "AboutDialog.h"
#include "SettingsDialog.h"
#include "Theme.h"
#include "UpdateDialog.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMenu>
#include <QPushButton>
#include <QStyleHints>
#include <QUrl>
#include <QVBoxLayout>

// --- ServiceCard ----------------------------------------------------------------------------------

ServiceCard::ServiceCard(const QString& title, QWidget* parent) : QFrame(parent)
{
    setObjectName("Card");
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(16, 13, 10, 13);
    row->setSpacing(12);

    m_dot = new QLabel;
    m_dot->setFixedSize(10, 10);
    row->addWidget(m_dot, 0, Qt::AlignTop | Qt::AlignLeft);
    row->itemAt(0)->widget()->setContentsMargins(0, 0, 0, 0);

    auto* text = new QVBoxLayout;
    text->setSpacing(2);
    auto* head = new QHBoxLayout;
    head->setSpacing(8);
    m_title = new QLabel(title);
    m_title->setObjectName("CardTitle");
    m_version = new QLabel;
    m_version->setObjectName("Muted");
    head->addWidget(m_title);
    head->addWidget(m_version);
    head->addStretch();
    text->addLayout(head);
    m_detail = new QLabel;
    m_detail->setObjectName("Small");
    text->addWidget(m_detail);
    m_message = new QLabel;
    m_message->setObjectName("Message");
    m_message->setWordWrap(true);
    m_message->hide();
    text->addWidget(m_message);
    row->addLayout(text, 1);

    m_buttons = new QWidget;
    auto* b = new QHBoxLayout(m_buttons);
    b->setContentsMargins(0, 0, 0, 0);
    b->setSpacing(2);
    row->addWidget(m_buttons, 0, Qt::AlignVCenter);
}

void ServiceCard::setStatus(const QColor& dot, const QString& version, const QString& detail, const QString& message)
{
    m_dot->setStyleSheet(QString("background:%1; border-radius:5px; margin-top:6px;").arg(theme::css(dot)));
    m_dot->setFixedSize(10, 16);
    m_version->setText(version);
    m_detail->setText(detail);
    m_message->setText(message);
    m_message->setVisible(!message.isEmpty());
}

QToolButton* ServiceCard::addButton(const QString& icon, const QString& tip)
{
    auto* tb = new QToolButton;
    tb->setToolTip(tip);
    tb->setIconSize(QSize(20, 20));
    tb->setCursor(Qt::PointingHandCursor);
    tb->setIcon(theme::icon(icon, theme::t().muted));
    m_buttons->layout()->addWidget(tb);
    m_icons << qMakePair(tb, icon);
    return tb;
}

void ServiceCard::refreshIcons()
{
    for (auto& [tb, name] : m_icons) tb->setIcon(theme::icon(tb->property("icon").toString().isEmpty() ? name : tb->property("icon").toString(), theme::t().muted));
}

// --- MainWindow -----------------------------------------------------------------------------------

MainWindow::MainWindow(Engine* engine, QWidget* parent) : QWidget(parent), m_engine(engine)
{
    setObjectName("Root");
    setWindowTitle("APM");
    setWindowIcon(QIcon(theme::appIcon(256)));
    buildUi();
    buildTray();
    connect(m_engine, &Engine::changed, this, &MainWindow::updateUi);
    connect(m_engine, &Engine::notify, this, [this](const QString& t, const QString& m) {
        if (!isVisible() && m_tray) m_tray->showMessage(t, m, QSystemTrayIcon::Warning, 6000);
    });
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        theme::apply(*qApp);
        refreshIcons();
        updateUi();
    });
    updateUi();
}

void MainWindow::buildUi()
{
    const Tokens& t = theme::t();
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 14);
    root->setSpacing(12);

    // header
    auto* head = new QHBoxLayout;
    head->setSpacing(12);
    m_logo = new QLabel;
    QPixmap logo = theme::appIcon(88);
    logo.setDevicePixelRatio(2);
    m_logo->setPixmap(logo);
    head->addWidget(m_logo);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(0);
    auto* title = new QLabel("APM");
    title->setObjectName("Title");
    m_rootLabel = new QLabel(QString("Apache · PHP · MariaDB   %1").arg(QDir::toNativeSeparators(m_engine->root())));
    m_rootLabel->setObjectName("Small");
    titles->addWidget(title);
    titles->addWidget(m_rootLabel);
    head->addLayout(titles, 1);
    m_pill = new QLabel;
    head->addWidget(m_pill, 0, Qt::AlignVCenter);
    root->addLayout(head);

    // primary actions
    auto* actions = new QHBoxLayout;
    actions->setSpacing(8);
    m_startAll = new QPushButton("Start all");
    m_startAll->setObjectName("Primary");
    m_startAll->setCursor(Qt::PointingHandCursor);
    m_stopAll = new QPushButton("Stop all");
    m_stopAll->setCursor(Qt::PointingHandCursor);
    connect(m_startAll, &QPushButton::clicked, m_engine, &Engine::startAll);
    connect(m_stopAll, &QPushButton::clicked, m_engine, &Engine::stopAll);
    actions->addWidget(m_startAll);
    actions->addWidget(m_stopAll);
    actions->addStretch();
    auto addLink = [this, &t](QBoxLayout* l, const QString& icon, const QString& text, auto slot) {
        auto* tb = new QToolButton;
        tb->setObjectName("Link");
        tb->setText(text);
        tb->setIcon(theme::icon(icon, t.muted));
        tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        tb->setCursor(Qt::PointingHandCursor);
        connect(tb, &QToolButton::clicked, this, slot);
        l->addWidget(tb);
        m_linkIcons << qMakePair(tb, icon);
        return tb;
    };
    addLink(actions, "globe", "Open site", [this] { openUrl(siteUrl()); });
    root->addLayout(actions);

    // cards
    m_apache = new ServiceCard("Apache");
    m_apacheToggle = m_apache->addButton("play", "Start");
    connect(m_apacheToggle, &QToolButton::clicked, this, [this] {
        const auto st = m_engine->status(Comp::Apache).state;
        st == CompStatus::State::Running ? m_engine->stop(Comp::Apache) : m_engine->start(Comp::Apache);
    });
    connect(m_apache->addButton("restart", "Restart (graceful)"), &QToolButton::clicked, this, [this] { m_engine->restart(Comp::Apache); });
    connect(m_apache->addButton("log", "Error log"), &QToolButton::clicked, this, [this] { openFile(m_engine->logPath(Comp::Apache)); });
    connect(m_apache->addButton("tune", "httpd.conf"), &QToolButton::clicked, this, [this] { openFile(m_engine->configPath(Comp::Apache)); });

    m_maria = new ServiceCard("MariaDB");
    m_mariaToggle = m_maria->addButton("play", "Start");
    connect(m_mariaToggle, &QToolButton::clicked, this, [this] {
        const auto st = m_engine->status(Comp::MariaDB).state;
        st == CompStatus::State::Running ? m_engine->stop(Comp::MariaDB) : m_engine->start(Comp::MariaDB);
    });
    connect(m_maria->addButton("restart", "Restart"), &QToolButton::clicked, this, [this] { m_engine->restart(Comp::MariaDB); });
    connect(m_maria->addButton("log", "Error log"), &QToolButton::clicked, this, [this] { openFile(m_engine->logPath(Comp::MariaDB)); });
    connect(m_maria->addButton("tune", "my.ini"), &QToolButton::clicked, this, [this] { openFile(m_engine->configPath(Comp::MariaDB)); });

    m_php = new ServiceCard("PHP");
    connect(m_php->addButton("log", "PHP error log"), &QToolButton::clicked, this, [this] { openFile(m_engine->phpLog()); });
    connect(m_php->addButton("tune", "php.ini"), &QToolButton::clicked, this, [this] { openFile(m_engine->phpIni()); });

    root->addWidget(m_apache);
    root->addWidget(m_maria);
    root->addWidget(m_php);

    // footer
    auto* foot = new QHBoxLayout;
    foot->setSpacing(2);
    addLink(foot, "folder", "www", [this] { openFile(m_engine->path("www")); });
    addLink(foot, "settings", "Settings", [this] {
        SettingsDialog d(m_engine, this);
        d.exec();
        updateUi();
    });
    addLink(foot, "update", "Update", [this] {
        UpdateDialog d(m_engine, this);
        d.exec();
        updateUi();
    });
    foot->addStretch();
    auto* about = new QToolButton;
    about->setObjectName("Link");
    about->setText(QString("v%1  ·  %2").arg(apmVersionShort(), credit::kAuthor));
    about->setToolTip("About APM - " + apmVersionLong());
    about->setCursor(Qt::PointingHandCursor);
    connect(about, &QToolButton::clicked, this, [this] {
        AboutDialog d(m_engine, this);
        d.exec();
    });
    foot->addWidget(about);
    root->addLayout(foot);

    setFixedWidth(500);
}

void MainWindow::buildTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
    m_tray = new QSystemTrayIcon(this);
    m_trayMenu = new QMenu(this);
    m_trayMenu->addAction("Open APM", this, &MainWindow::showNormalAndRaise);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction("Start all", m_engine, &Engine::startAll);
    m_trayMenu->addAction("Stop all", m_engine, &Engine::stopAll);
    m_trayMenu->addAction("Restart Apache", this, [this] { m_engine->restart(Comp::Apache); });
    m_trayMenu->addSeparator();
    m_trayMenu->addAction("Open site", this, [this] { openUrl(siteUrl()); });
    m_trayMenu->addAction("Open www folder", this, [this] { openFile(m_engine->path("www")); });
    m_trayMenu->addSeparator();
    m_trayMenu->addAction("Quit (servers keep running)", qApp, &QApplication::quit);
    m_tray->setContextMenu(m_trayMenu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick)
            isVisible() && !isMinimized() ? hide() : showNormalAndRaise();
    });
    m_tray->show();
}

QColor MainWindow::dotColor(const CompStatus& s) const
{
    const Tokens& t = theme::t();
    switch (s.state) {
    case CompStatus::State::Running: return t.ok;
    case CompStatus::State::Starting:
    case CompStatus::State::Stopping: return t.warn;
    case CompStatus::State::Missing: return t.bad;
    default: return s.message.isEmpty() ? t.idle : t.bad;
    }
}

QString MainWindow::stateText(const CompStatus& s) const
{
    switch (s.state) {
    case CompStatus::State::Running: return "Running";
    case CompStatus::State::Starting: return "Starting...";
    case CompStatus::State::Stopping: return "Stopping...";
    case CompStatus::State::Missing: return "Not installed";
    default: return "Stopped";
    }
}

void MainWindow::updateUi()
{
    const Tokens& t = theme::t();
    const CompStatus& a = m_engine->status(Comp::Apache);
    const CompStatus& m = m_engine->status(Comp::MariaDB);

    auto ports = [](const QList<quint16>& p) {
        QStringList s;
        for (quint16 v : p) s << QString::number(v);
        return s.join(", ");
    };
    auto mode = [](const CompStatus& s) {
        return s.service ? QString("Service, %1").arg(s.serviceAuto ? "starts with Windows" : "manual") : QString("Process");
    };

    m_apache->setStatus(dotColor(a), a.version,
                        QString("%1  ·  ports %2  ·  %3").arg(stateText(a), ports(a.ports), mode(a)), a.message);
    m_maria->setStatus(dotColor(m), m.version,
                       QString("%1  ·  port %2 (this PC only)  ·  %3").arg(stateText(m), ports(m.ports), mode(m)), m.message);
    const bool phpUp = a.state == CompStatus::State::Running;
    m_php->setStatus(phpUp ? t.ok : t.idle, m_engine->phpVersion(),
                     QString("FastCGI through Apache  ·  %1 worker%2").arg(m_engine->phpWorkers()).arg(m_engine->phpWorkers() == 1 ? "" : "s"),
                     QString());

    auto setToggle = [](QToolButton* tb, const CompStatus& s) {
        const bool run = s.state == CompStatus::State::Running;
        tb->setProperty("icon", run ? "stop" : "play");
        tb->setIcon(theme::icon(run ? "stop" : "play", run ? theme::t().bad : theme::t().ok));
        tb->setToolTip(run ? "Stop" : "Start");
        tb->setEnabled(s.state == CompStatus::State::Running || s.state == CompStatus::State::Stopped);
    };
    setToggle(m_apacheToggle, a);
    setToggle(m_mariaToggle, m);

    const int up = (a.state == CompStatus::State::Running) + (m.state == CompStatus::State::Running);
    const bool busy = m_engine->busy();
    QColor pc = up == 2 ? t.ok : up == 1 ? t.warn : t.idle;
    QString pt = busy ? "Working..." : up == 2 ? "Running" : up == 1 ? "Partly running" : "Stopped";
    if (busy) pc = t.warn;
    QColor bgc = pc;
    bgc.setAlphaF(t.dark ? 0.18 : 0.13);
    m_pill->setText(pt);
    m_pill->setStyleSheet(QString("background: rgba(%1,%2,%3,%4); color: %5; border-radius: 11px; padding: 4px 12px; font-weight: 600;")
                              .arg(bgc.red()).arg(bgc.green()).arg(bgc.blue()).arg(bgc.alphaF()).arg(theme::css(pc)));
    m_startAll->setEnabled(up < 2 && !busy);
    m_stopAll->setEnabled(up > 0 && !busy);

    if (m_tray) {
        const QString key = pt;
        if (key != m_trayState) {
            m_trayState = key;
            m_tray->setIcon(QIcon(theme::appIcon(64, pc)));
        }
        m_tray->setToolTip(QString("APM - %1\nApache: %2\nMariaDB: %3").arg(pt, stateText(a), stateText(m)));
    }
}

void MainWindow::refreshIcons()
{
    for (auto& [tb, name] : m_linkIcons) tb->setIcon(theme::icon(name, theme::t().muted));
    m_apache->refreshIcons();
    m_maria->refreshIcons();
    m_php->refreshIcons();
    m_trayState.clear();
}

QString MainWindow::siteUrl() const
{
    const quint16 p = m_engine->httpPort();
    return p == 80 ? QString("http://localhost/") : QString("http://localhost:%1/").arg(p);
}

void MainWindow::openFile(const QString& path)
{
    if (!QFileInfo::exists(path)) {
        if (m_tray) m_tray->showMessage("APM", "Not found: " + QDir::toNativeSeparators(path), QSystemTrayIcon::Information, 4000);
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::openUrl(const QString& url) { QDesktopServices::openUrl(QUrl(url)); }

void MainWindow::showNormalAndRaise()
{
    show();
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    raise();
    activateWindow();
}

void MainWindow::startHidden()
{
    if (!m_tray) show();
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    if (m_tray && m_tray->isVisible()) {
        hide();
        if (!m_balloonShown) {
            m_tray->showMessage("APM is still running", "Servers keep running. Open APM from the tray icon.", QSystemTrayIcon::Information, 4000);
            m_balloonShown = true;
        }
        e->ignore();
        return;
    }
    QWidget::closeEvent(e);
}

void MainWindow::changeEvent(QEvent* e)
{
    // minimise goes to the tray instead of the taskbar
    if (e->type() == QEvent::WindowStateChange && isMinimized() && m_tray) QTimer::singleShot(0, this, &QWidget::hide);
    QWidget::changeEvent(e);
}
