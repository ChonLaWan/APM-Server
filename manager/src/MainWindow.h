// MainWindow.h - compact control window + tray icon.
#pragma once
#include "Engine.h"

#include <QFrame>
#include <QLabel>
#include <QSystemTrayIcon>
#include <QToolButton>
#include <QWidget>

class QPushButton;
class QMenu;

class ServiceCard : public QFrame {
    Q_OBJECT
public:
    ServiceCard(const QString& title, QWidget* parent = nullptr);
    void setStatus(const QColor& dot, const QString& version, const QString& detail, const QString& message);
    QToolButton* addButton(const QString& icon, const QString& tip);
    void refreshIcons();

private:
    QLabel* m_dot;
    QLabel* m_title;
    QLabel* m_version;
    QLabel* m_detail;
    QLabel* m_message;
    QWidget* m_buttons;
    QList<QPair<QToolButton*, QString>> m_icons;
};

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(Engine* engine, QWidget* parent = nullptr);
    void showNormalAndRaise();
    void startHidden();

protected:
    void closeEvent(QCloseEvent* e) override;
    void changeEvent(QEvent* e) override;

private:
    void buildUi();
    void buildTray();
    void updateUi();
    void refreshIcons();
    void openFile(const QString& path);
    void openUrl(const QString& url);
    QString siteUrl() const;
    QColor dotColor(const CompStatus& s) const;
    QString stateText(const CompStatus& s) const;

    Engine* m_engine;
    QLabel* m_logo = nullptr;
    QLabel* m_rootLabel = nullptr;
    QLabel* m_pill = nullptr;
    QPushButton* m_startAll = nullptr;
    QPushButton* m_stopAll = nullptr;
    ServiceCard* m_apache = nullptr;
    ServiceCard* m_maria = nullptr;
    ServiceCard* m_php = nullptr;
    QToolButton* m_apacheToggle = nullptr;
    QToolButton* m_mariaToggle = nullptr;
    QList<QPair<QToolButton*, QString>> m_linkIcons;
    QSystemTrayIcon* m_tray = nullptr;
    QMenu* m_trayMenu = nullptr;
    QString m_trayState;
    bool m_balloonShown = false;
};
