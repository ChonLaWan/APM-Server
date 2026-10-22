// UpdateDialog.h - compare installed engine versions with upstream, and apply a newer APM package
// (zip or unpacked folder). Binaries are replaced; config, data, www, ssl and logs are kept.
#pragma once
#include <QDialog>
#include <QStringList>

class Engine;
class QCheckBox;
class QLabel;
class QProgressBar;
class QLineEdit;
class QNetworkAccessManager;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

class UpdateDialog : public QDialog {
    Q_OBJECT
public:
    explicit UpdateDialog(Engine* engine, QWidget* parent = nullptr);

private:
    QString installedApmVersion() const;
    void checkGitHub();
    void downloadRelease();
    void fillInstalled();
    void checkOnline();
    void apply();
    void runSteps();
    void finish(bool ok);
    QString packageRoot(const QString& dir) const;
    void log(const QString& line);

    Engine* m_engine;
    QLabel* m_relInfo;
    QLabel* m_relNotes;
    QProgressBar* m_progress;
    QPushButton* m_relInstall;
    QString m_relZip, m_relZipName, m_relSha;
    QTableWidget* m_table;
    QLineEdit* m_source;
    QCheckBox* m_refreshConfig;
    QPushButton* m_apply;
    QPlainTextEdit* m_log;
    QNetworkAccessManager* m_net;
    QString m_pkg;
    QList<QStringList> m_steps;   // robocopy argument lists
    bool m_wasApache = false, m_wasMaria = false;
    bool m_managerUpdated = false;
};
