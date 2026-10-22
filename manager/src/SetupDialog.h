// SetupDialog.h - first run of an unpacked APM folder (or after moving it): ports, LAN, then setup::run.
#pragma once
#include <QDialog>

class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

class SetupDialog : public QDialog {
    Q_OBJECT
public:
    explicit SetupDialog(const QString& root, QWidget* parent = nullptr);

private:
    void checkPorts();
    void runSetup();

    QString m_root;
    QSpinBox* m_http;
    QSpinBox* m_https;
    QSpinBox* m_db;
    QLabel* m_portNote;
    QCheckBox* m_lan;
    QPlainTextEdit* m_log;
    QPushButton* m_go;
    bool m_existing = false;
};
