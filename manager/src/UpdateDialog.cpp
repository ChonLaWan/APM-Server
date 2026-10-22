#include "UpdateDialog.h"
#include "AboutDialog.h"
#include "Engine.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QProgressBar>
#include "Setup.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVersionNumber>

namespace {
enum Row { RowPhp, RowApache, RowMaria };
QString nat(const QString& p) { return QDir::toNativeSeparators(p); }
}

UpdateDialog::UpdateDialog(Engine* engine, QWidget* parent) : QDialog(parent), m_engine(engine)
{
    setWindowTitle("APM Updates");
    setMinimumWidth(560);
    m_net = new QNetworkAccessManager(this);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 18, 20, 16);
    v->setSpacing(8);

    // --- 1. APM releases on GitHub (the normal way to update) ---------------------------------
    auto* secRel = new QLabel("APM UPDATES");
    secRel->setObjectName("Section");
    v->addWidget(secRel);
    m_relInfo = new QLabel(QString("Installed: APM %1 (manager %2). Updates come from %3/releases")
                               .arg(installedApmVersion(), apmVersionShort(), QString(credit::kRepo).remove("https://")));
    m_relInfo->setWordWrap(true);
    m_relInfo->setTextFormat(Qt::PlainText);
    v->addWidget(m_relInfo);
    m_relNotes = new QLabel;
    m_relNotes->setObjectName("Small");
    m_relNotes->setWordWrap(true);
    m_relNotes->hide();
    v->addWidget(m_relNotes);
    m_progress = new QProgressBar;
    m_progress->setTextVisible(true);
    m_progress->hide();
    v->addWidget(m_progress);
    auto* relRow = new QHBoxLayout;
    auto* checkRel = new QPushButton("Check for updates");
    m_relInstall = new QPushButton("Download and install");
    m_relInstall->setObjectName("Primary");
    m_relInstall->hide();
    relRow->addWidget(checkRel);
    relRow->addWidget(m_relInstall);
    relRow->addStretch();
    v->addLayout(relRow);
    connect(checkRel, &QPushButton::clicked, this, &UpdateDialog::checkGitHub);
    connect(m_relInstall, &QPushButton::clicked, this, &UpdateDialog::downloadRelease);

    // --- 2. what upstream has (tells whether a new APM build is worth making) --------------------
    v->addSpacing(8);
    auto* sec = new QLabel("ENGINE VERSIONS");
    sec->setObjectName("Section");
    v->addWidget(sec);
    m_table = new QTableWidget(3, 3);
    m_table->setHorizontalHeaderLabels({"Component", "Installed", "Latest upstream"});
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setFixedHeight(m_table->horizontalHeader()->height() + 3 * 30 + 4);
    for (int r = 0; r < 3; ++r) m_table->setRowHeight(r, 30);
    v->addWidget(m_table);
    auto* check = new QPushButton("Check upstream");
    connect(check, &QPushButton::clicked, this, &UpdateDialog::checkOnline);
    v->addWidget(check, 0, Qt::AlignLeft);

    // --- 3. offline / developer: a zip or an unpacked folder ----------------------------------------
    v->addSpacing(8);
    auto* sec2 = new QLabel("INSTALL FROM A FILE");
    sec2->setObjectName("Section");
    v->addWidget(sec2);
    auto* help = new QLabel("An APM zip or unpacked folder (developers: dist\\APM-... from build.cmd). "
                            "Either way, servers are stopped, binaries replaced and servers started again. Your config, "
                            "databases, www, certificates and logs are not touched.");
    help->setObjectName("Small");
    help->setWordWrap(true);
    v->addWidget(help);
    auto* row = new QHBoxLayout;
    m_source = new QLineEdit(QSettings("APM", "Manager").value("updateSource").toString());
    m_source->setPlaceholderText("APM-x.y.z.zip or folder");
    auto* zipBtn = new QPushButton("Zip...");
    auto* dirBtn = new QPushButton("Folder...");
    row->addWidget(m_source, 1);
    row->addWidget(zipBtn);
    row->addWidget(dirBtn);
    v->addLayout(row);
    connect(zipBtn, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(this, "APM package", QString(), "APM package (*.zip)");
        if (!f.isEmpty()) m_source->setText(nat(f));
    });
    connect(dirBtn, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, "Unpacked APM folder");
        if (!d.isEmpty()) m_source->setText(nat(d));
    });
    m_refreshConfig = new QCheckBox("Also re-create config files from the new templates (ports and LAN setting are kept)");
    v->addWidget(m_refreshConfig);

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMinimumHeight(150);
    v->addWidget(m_log);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    m_apply = new QPushButton("Install from file");
    auto* close = new QPushButton("Close");
    buttons->addWidget(m_apply);
    buttons->addWidget(close);
    v->addLayout(buttons);
    connect(m_apply, &QPushButton::clicked, this, &UpdateDialog::apply);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);

    fillInstalled();
}

QString UpdateDialog::installedApmVersion() const
{
    // package version lives in <root>\versions.json; an older install may lack it -> the manager's own version
    QFile f(m_engine->path("versions.json"));
    const QString v = f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object().value("apm_version").toString() : QString();
    return v.isEmpty() ? QString(APM_MANAGER_VERSION) : v;
}

void UpdateDialog::checkGitHub()
{
    const QString repo = QString(credit::kRepo).remove("https://github.com/");
    QNetworkRequest req(QUrl("https://api.github.com/repos/" + repo + "/releases/latest"));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setHeader(QNetworkRequest::UserAgentHeader, QString("APM-Manager/%1").arg(apmVersionShort()));
    m_relInfo->setText("Checking GitHub...");
    m_relInstall->hide();
    m_relNotes->hide();
    QNetworkReply* r = m_net->get(req);
    connect(r, &QNetworkReply::finished, this, [this, r] {
        r->deleteLater();
        const int http = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (http == 404) { m_relInfo->setText("No APM release is published yet (or the repository is private)."); return; }
        if (r->error() != QNetworkReply::NoError) { m_relInfo->setText("Could not reach GitHub: " + r->errorString()); return; }
        const QJsonObject rel = QJsonDocument::fromJson(r->readAll()).object();
        const QString tag = rel.value("tag_name").toString();
        const QString latest = tag.startsWith('v') ? tag.mid(1) : tag;
        m_relZip.clear();
        m_relSha.clear();
        qint64 size = 0;
        static const QRegularExpression zipName(R"(^APM-.*-win64\.zip$)");
        for (const QJsonValue& a : rel.value("assets").toArray()) {
            const QJsonObject o = a.toObject();
            const QString name = o.value("name").toString();
            if (zipName.match(name).hasMatch()) { m_relZip = o.value("browser_download_url").toString(); size = o.value("size").toInteger(); m_relZipName = name; }
            else if (name.endsWith(".zip.sha256")) m_relSha = o.value("browser_download_url").toString();
        }
        const QString have = installedApmVersion();
        const bool newer = QVersionNumber::fromString(latest) > QVersionNumber::fromString(have);
        const QString date = rel.value("published_at").toString().left(10);
        if (!newer) {
            m_relInfo->setText(QString("APM is up to date (installed %1, latest release %2 from %3).").arg(have, latest, date));
            return;
        }
        if (m_relZip.isEmpty()) { m_relInfo->setText(QString("Release %1 has no APM-*-win64.zip attached yet.").arg(tag)); return; }
        m_relInfo->setText(QString("Update available: APM %1 (%2), installed %3.").arg(latest, date, have));
        QString notes = rel.value("body").toString().trimmed();
        if (notes.size() > 600) notes = notes.left(600) + " ...";
        m_relNotes->setText(notes);
        m_relNotes->setVisible(!notes.isEmpty());
        m_relInstall->setText(QString("Download and install (%1 MB)").arg(size / (1024 * 1024)));
        m_relInstall->show();
    });
}

void UpdateDialog::downloadRelease()
{
    if (m_relZip.isEmpty()) return;
    const QString dir = m_engine->path("tmp");
    QDir().mkpath(dir);
    auto* file = new QFile(dir + "/" + m_relZipName, this);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) { log("[ERR] cannot write " + nat(file->fileName())); return; }
    m_relInstall->setEnabled(false);
    m_apply->setEnabled(false);
    m_progress->setValue(0);
    m_progress->show();
    log("[..] downloading " + m_relZip);
    QNetworkRequest req{QUrl(m_relZip)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QString("APM-Manager/%1").arg(apmVersionShort()));
    QNetworkReply* r = m_net->get(req);
    connect(r, &QNetworkReply::readyRead, this, [r, file] { file->write(r->readAll()); });
    connect(r, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
        if (total > 0) { m_progress->setMaximum(int(total / 1024)); m_progress->setValue(int(got / 1024)); }
    });
    connect(r, &QNetworkReply::finished, this, [this, r, file] {
        r->deleteLater();
        file->write(r->readAll());
        file->close();
        const QString path = file->fileName();
        file->deleteLater();
        auto fail = [this, path](const QString& why) {
            log("[ERR] " + why);
            QFile::remove(path);
            m_relInstall->setEnabled(true);
            m_apply->setEnabled(true);
            m_progress->hide();
        };
        if (r->error() != QNetworkReply::NoError) { fail("download failed: " + r->errorString()); return; }
        log("[OK] downloaded " + nat(path));
        auto install = [this, path] {
            m_progress->hide();
            m_source->setText(nat(path));
            apply();
        };
        if (m_relSha.isEmpty()) { log("[WARN] release has no .sha256 - not verified"); install(); return; }
        QNetworkRequest sreq{QUrl(m_relSha)};
        sreq.setHeader(QNetworkRequest::UserAgentHeader, "APM-Manager");
        QNetworkReply* s = m_net->get(sreq);
        connect(s, &QNetworkReply::finished, this, [this, s, path, install, fail] {
            s->deleteLater();
            const QString want = QString::fromUtf8(s->readAll()).trimmed().section(' ', 0, 0).toLower();
            QFile f(path);
            QCryptographicHash h(QCryptographicHash::Sha256);
            if (!f.open(QIODevice::ReadOnly) || !h.addData(&f)) { fail("cannot read the download"); return; }
            const QString got = QString::fromLatin1(h.result().toHex());
            if (want.isEmpty() || got != want) { fail("sha256 mismatch - download discarded"); return; }
            log("[OK] sha256 verified " + got);
            install();
        });
    });
}

void UpdateDialog::fillInstalled()
{
    auto set = [this](int r, const QString& name, const QString& ver) {
        m_table->setItem(r, 0, new QTableWidgetItem(name));
        m_table->setItem(r, 1, new QTableWidgetItem(ver.isEmpty() ? "-" : ver));
        if (!m_table->item(r, 2)) m_table->setItem(r, 2, new QTableWidgetItem("-"));
    };
    set(RowPhp, "PHP", m_engine->phpVersion());
    set(RowApache, "Apache httpd", m_engine->status(Comp::Apache).version);
    set(RowMaria, "MariaDB", m_engine->status(Comp::MariaDB).version);
}

void UpdateDialog::checkOnline()
{
    auto fetch = [this](int row, const QString& url, std::function<QString(const QByteArray&)> parse) {
        m_table->item(row, 2)->setText("checking...");
        QNetworkReply* r = m_net->get(QNetworkRequest(QUrl(url)));
        connect(r, &QNetworkReply::finished, this, [this, r, row, parse] {
            const QString latest = r->error() == QNetworkReply::NoError ? parse(r->readAll()) : QString();
            r->deleteLater();
            auto* item = m_table->item(row, 2);
            const QString have = m_table->item(row, 1)->text();
            if (latest.isEmpty()) { item->setText("unavailable"); return; }
            const bool newer = QVersionNumber::fromString(latest) > QVersionNumber::fromString(have);
            item->setText(newer ? latest + "  (update available)" : latest + "  (up to date)");
            item->setForeground(newer ? QColor("#d97706") : QColor("#12a150"));
        });
    };
    const QVersionNumber php = QVersionNumber::fromString(m_engine->phpVersion());
    const QString phpSeries = php.isNull() ? "8.4" : QString("%1.%2").arg(php.majorVersion()).arg(php.minorVersion());
    fetch(RowPhp, "https://www.php.net/releases/index.php?json&max=1&version=" + phpSeries, [](const QByteArray& b) {
        const QJsonObject o = QJsonDocument::fromJson(b).object();
        return o.isEmpty() ? QString() : o.keys().value(0);
    });
    fetch(RowApache, "https://downloads.apache.org/httpd/", [](const QByteArray& b) {
        QVersionNumber best;
        static const QRegularExpression re(R"(httpd-(2\.4\.\d+)\.tar\.bz2)");
        for (auto it = re.globalMatch(QString::fromUtf8(b)); it.hasNext();) best = std::max(best, QVersionNumber::fromString(it.next().captured(1)));
        return best.isNull() ? QString() : best.toString();
    });
    const QVersionNumber maria = QVersionNumber::fromString(m_engine->status(Comp::MariaDB).version);
    const QString series = maria.isNull() ? "11.8" : QString("%1.%2").arg(maria.majorVersion()).arg(maria.minorVersion());
    fetch(RowMaria, "https://downloads.mariadb.org/rest-api/mariadb/" + series + "/latest/", [](const QByteArray& b) {
        static const QRegularExpression re(R"re("release_id"\s*:\s*"([\d.]+)")re");
        const auto m = re.match(QString::fromUtf8(b));
        return m.hasMatch() ? m.captured(1) : QString();
    });
}

void UpdateDialog::log(const QString& line) { m_log->appendPlainText(line); }

QString UpdateDialog::packageRoot(const QString& dir) const
{
    if (QFileInfo::exists(dir + "/apache/bin/httpd.exe")) return dir;
    QDirIterator it(dir, QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        const QString d = it.next();
        if (QFileInfo::exists(d + "/apache/bin/httpd.exe")) return d;
    }
    return {};
}

void UpdateDialog::apply()
{
    const QString src = QDir::fromNativeSeparators(m_source->text().trimmed());
    if (src.isEmpty() || !QFileInfo::exists(src)) { log("[ERR] choose a package zip or folder first"); return; }
    QSettings("APM", "Manager").setValue("updateSource", nat(src));
    const QString root = m_engine->root();
    if (QDir::cleanPath(src).compare(root, Qt::CaseInsensitive) == 0) { log("[ERR] that is the installed APM folder itself"); return; }

    m_apply->setEnabled(false);
    QString dir = src;
    if (src.endsWith(".zip", Qt::CaseInsensitive)) {
        dir = root + "/tmp/update-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
        QDir().mkpath(dir);
        log("[..] unpacking " + nat(src));
        QProcess tar;
        tar.start("tar.exe", {"-xf", nat(src), "-C", nat(dir)});
        if (!tar.waitForFinished(600000) || tar.exitCode() != 0) { log("[ERR] could not unpack the zip"); finish(false); return; }
    }
    m_pkg = packageRoot(dir);
    if (m_pkg.isEmpty()) { log("[ERR] no APM package found in " + nat(dir)); finish(false); return; }

    // robocopy: /MIR mirrors binaries; excluded names are protected from deletion on the target side too
    const QStringList common = {"/R:1", "/W:1", "/NFL", "/NDL", "/NJH", "/NJS", "/NP"};
    m_steps.clear();
    m_steps << (QStringList{nat(m_pkg + "/apache"), nat(root + "/apache"), "/MIR", "/XD", "conf", "logs", "include", "lib", "/XF", "*.pdb", "*.lib", "*.exp"} + common);
    m_steps << (QStringList{nat(m_pkg + "/apache/conf"), nat(root + "/apache/conf"), "mime.types", "magic", "charset.conv"} + common);
    m_steps << (QStringList{nat(m_pkg + "/php"), nat(root + "/php"), "/MIR", "/XF", "php.ini", "*.pdb", "*.lib", "*.exp"} + common);
    m_steps << (QStringList{nat(m_pkg + "/mariadb"), nat(root + "/mariadb"), "/MIR", "/XD", "include", "symbols", "sql-bench", "/XF", "my.ini", "*.pdb", "*.lib", "*.exp"} + common);
    for (const char* d : {"templates", "tools"})
        if (QFileInfo::exists(m_pkg + "/" + d)) m_steps << (QStringList{nat(m_pkg + "/" + d), nat(root + "/" + d), "/MIR"} + common);
    m_steps << (QStringList{nat(m_pkg), nat(root), "versions.json", "README.txt"} + common);
    if (QFileInfo::exists(m_pkg + "/ssl/cacert.pem")) m_steps << (QStringList{nat(m_pkg + "/ssl"), nat(root + "/ssl"), "cacert.pem"} + common);

    m_wasApache = m_engine->status(Comp::Apache).state == CompStatus::State::Running;
    m_wasMaria = m_engine->status(Comp::MariaDB).state == CompStatus::State::Running;
    log("[..] stopping servers");
    m_engine->stopAllThen([this](bool ok) {
        if (!ok) { log("[ERR] servers did not stop - update cancelled"); finish(false); return; }
        log("[OK] servers stopped");
        runSteps();
    });
}

void UpdateDialog::runSteps()
{
    if (m_steps.isEmpty()) {
        // the manager itself: running files cannot be overwritten but can be renamed -> *.old, cleaned at next start
        const QString mine = QCoreApplication::applicationDirPath();
        const QString theirs = m_pkg + "/manager";
        if (QFileInfo::exists(theirs + "/APMManager.exe") && QDir(mine).absolutePath().compare(QDir(theirs).absolutePath(), Qt::CaseInsensitive) != 0) {
            QDirIterator it(theirs, QDir::Files, QDirIterator::Subdirectories);
            int n = 0;
            while (it.hasNext()) {
                const QString f = it.next();
                const QString dest = mine + "/" + QDir(theirs).relativeFilePath(f);
                QDir().mkpath(QFileInfo(dest).absolutePath());
                if (QFileInfo::exists(dest)) {
                    QFile::remove(dest + ".old");
                    QFile::rename(dest, dest + ".old");
                }
                if (QFile::copy(f, dest)) ++n;
            }
            m_managerUpdated = true;
            log(QString("[OK] manager files updated (%1) - restart APM Manager to use the new version").arg(n));
        }
        if (m_refreshConfig->isChecked()) {
            QStringList out;
            const bool ok = setup::run(setup::load(m_engine->root()), true, &out);
            for (const QString& l : out) log(l);
            if (!ok) { finish(false); return; }
        }
        finish(true);
        return;
    }
    const QStringList args = m_steps.takeFirst();
    log("[..] " + QFileInfo(args.value(1)).fileName() + (args.size() > 2 && !args[2].startsWith('/') ? " (" + args[2] + ")" : ""));
    auto* p = new QProcess(this);
    connect(p, &QProcess::finished, this, [this, p](int code) {
        p->deleteLater();
        if (code >= 8) { log(QString("[ERR] robocopy failed (exit %1)").arg(code)); finish(false); return; }
        runSteps();
    });
    p->start("robocopy.exe", args);
}

void UpdateDialog::finish(bool ok)
{
    log(ok ? "[OK] update applied" : "[ERR] update stopped - starting the previous servers again");
    m_engine->refresh();
    if (m_wasMaria) m_engine->start(Comp::MariaDB);
    if (m_wasApache) m_engine->start(Comp::Apache);
    m_apply->setEnabled(true);
    fillInstalled();
    if (ok && m_managerUpdated &&
        QMessageBox::question(this, "APM", "APM Manager was updated. Restart it now?") == QMessageBox::Yes) {
        QProcess::startDetached(QCoreApplication::applicationFilePath(), {"--restarted"});
        QCoreApplication::quit();
    }
}
