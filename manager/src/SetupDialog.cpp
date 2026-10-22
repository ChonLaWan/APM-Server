#include "SetupDialog.h"
#include "Setup.h"
#include "Theme.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

SetupDialog::SetupDialog(const QString& root, QWidget* parent) : QDialog(parent), m_root(root)
{
    setWindowTitle("Set up APM");
    setWindowIcon(QIcon(theme::appIcon(256)));
    setMinimumWidth(500);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(22, 20, 22, 16);
    v->setSpacing(10);

    auto* head = new QHBoxLayout;
    auto* logo = new QLabel;
    QPixmap pm = theme::appIcon(96);
    pm.setDevicePixelRatio(2);
    logo->setPixmap(pm);
    head->addWidget(logo);
    auto* titles = new QVBoxLayout;
    auto* title = new QLabel("Set up APM");
    title->setObjectName("Title");
    auto* where = new QLabel(QDir::toNativeSeparators(root));
    where->setObjectName("Small");
    titles->addWidget(title);
    titles->addWidget(where);
    head->addLayout(titles, 1);
    v->addLayout(head);

    // an older deploy (no apm.json) or a moved folder: start from what is configured there
    m_existing = QFileInfo::exists(root + "/apache/conf/httpd.conf");
    const ApmConfig cfg = m_existing ? setup::load(root) : setup::suggest(root);
    auto* intro = new QLabel(setup::wasMoved(root)
                                 ? "This APM folder was moved or copied. Its config still points to the old place and will be re-created here."
                                 : m_existing ? "Finish setting up this APM folder."
                                              : "Choose ports, then APM creates its config, a local HTTPS certificate and an empty MariaDB database.");
    intro->setWordWrap(true);
    v->addWidget(intro);

    auto* ports = new QHBoxLayout;
    auto spin = [](int val) {
        auto* s = new QSpinBox;
        s->setRange(1, 65535);
        s->setValue(val);
        s->setFixedWidth(84);
        s->setButtonSymbols(QAbstractSpinBox::NoButtons);
        return s;
    };
    m_http = spin(cfg.httpPort);
    m_https = spin(cfg.httpsPort);
    m_db = spin(cfg.dbPort);
    ports->addWidget(new QLabel("HTTP"));
    ports->addWidget(m_http);
    ports->addSpacing(8);
    ports->addWidget(new QLabel("HTTPS"));
    ports->addWidget(m_https);
    ports->addSpacing(8);
    ports->addWidget(new QLabel("MariaDB"));
    ports->addWidget(m_db);
    ports->addStretch();
    v->addLayout(ports);
    m_portNote = new QLabel;
    m_portNote->setObjectName("Message");
    m_portNote->setWordWrap(true);
    v->addWidget(m_portNote);
    for (QSpinBox* s : {m_http, m_https, m_db}) connect(s, &QSpinBox::valueChanged, this, &SetupDialog::checkPorts);

    m_lan = new QCheckBox("Let other computers on my network open the sites (the firewall can be opened later in Settings)");
    m_lan->setChecked(cfg.lan);
    v->addWidget(m_lan);

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMinimumHeight(140);
    m_log->hide();
    v->addWidget(m_log);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    m_go = new QPushButton("Set up");
    m_go->setObjectName("Primary");
    auto* cancel = new QPushButton("Cancel");
    buttons->addWidget(m_go);
    buttons->addWidget(cancel);
    v->addLayout(buttons);
    connect(m_go, &QPushButton::clicked, this, &SetupDialog::runSetup);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    checkPorts();
}

void SetupDialog::checkPorts()
{
    QStringList busy;
    for (QSpinBox* s : {m_http, m_https, m_db}) {
        const QString owner = setup::busyPortOwner(s->value());
        if (!owner.isEmpty() && !owner.contains("httpd") && !owner.contains("mariadbd"))
            busy << QString("port %1 is used by %2").arg(s->value()).arg(owner);
    }
    m_portNote->setText(busy.isEmpty() ? QString() : "Note: " + busy.join(", ") + ". Pick another port or stop that program.");
    m_portNote->setVisible(!busy.isEmpty());
}

void SetupDialog::runSetup()
{
    ApmConfig c;
    c.root = m_root;
    c.httpPort = m_http->value();
    c.httpsPort = m_https->value();
    c.dbPort = m_db->value();
    c.lan = m_lan->isChecked();
    const ApmConfig was = setup::load(m_root);
    // keep hand-edited config unless the ports/LAN changed or the folder moved
    const bool reset = !m_existing || setup::wasMoved(m_root) || was.httpPort != c.httpPort ||
                       was.httpsPort != c.httpsPort || was.dbPort != c.dbPort || was.lan != c.lan;
    m_go->setEnabled(false);
    m_log->show();
    m_log->appendPlainText("[..] setting up");
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QStringList out;
    const bool ok = setup::run(c, reset, &out);
    QApplication::restoreOverrideCursor();
    for (const QString& l : out) m_log->appendPlainText(l);
    if (ok) {
        m_go->setText("Open APM");
        m_go->setEnabled(true);
        disconnect(m_go, nullptr, this, nullptr);
        connect(m_go, &QPushButton::clicked, this, &QDialog::accept);
    } else {
        m_go->setEnabled(true);
    }
}
