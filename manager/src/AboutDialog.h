// AboutDialog.h - version, engine versions, credit, links.
#pragma once
#include <QDialog>
#include <QString>

class Engine;

class AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(Engine* engine, QWidget* parent = nullptr);

private:
    void showLicense(Engine* engine);
};

namespace credit {
inline constexpr const char* kAuthor = "ChonLaWan";
inline constexpr const char* kEmail = "wanpean@gmail.com";
inline constexpr const char* kWebsite = "https://wanpean.com";
inline constexpr const char* kRepo = "https://github.com/ChonLaWan/APM-Server";
} // namespace credit

// "1.0.0 build 3"  /  "1.0.0 build 3 (652326e, 2026-10-22)"
inline QString apmVersionShort() { return QString("%1 build %2").arg(APM_MANAGER_VERSION, APM_BUILD); }
inline QString apmVersionLong() { return apmVersionShort() + QString(" (%1, %2)").arg(APM_COMMIT, APM_BUILD_DATE); }
