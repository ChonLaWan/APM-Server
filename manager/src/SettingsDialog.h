// SettingsDialog.h - startup, Windows services, ports/LAN, PATH for VS Code, certificate trust.
#pragma once
#include <QDialog>
#include <functional>

class Engine;
class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(Engine* engine, QWidget* parent = nullptr);

    static bool runAtSignIn();
    static void setRunAtSignIn(bool on);
    static bool startServersOnOpen();

private:
    void refresh();
    void runElevatedOp(const QStringList& op, std::function<void(bool)> after = {});
    void applyNetwork();
    void log(const QStringList& lines);

    Engine* m_engine;
    QCheckBox* m_signIn;
    QCheckBox* m_autoServers;
    QLabel* m_svcState;
    QPushButton* m_svcInstall;
    QPushButton* m_svcRemove;
    QCheckBox* m_svcBoot;
    QSpinBox* m_http;
    QSpinBox* m_https;
    QSpinBox* m_db;
    QCheckBox* m_lan;
    QPushButton* m_applyNet;
    QLabel* m_pathState;
    QPushButton* m_pathAdd;
    QPushButton* m_pathRemove;
    QPlainTextEdit* m_log;
    bool m_updating = false;
};
