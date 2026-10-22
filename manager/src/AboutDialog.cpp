#include "AboutDialog.h"
#include "Engine.h"
#include "Theme.h"

#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

AboutDialog::AboutDialog(Engine* engine, QWidget* parent) : QDialog(parent)
{
    setWindowTitle("About APM");
    setMinimumWidth(420);
    const Tokens& t = theme::t();
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(24, 22, 24, 18);
    v->setSpacing(10);

    auto* head = new QHBoxLayout;
    head->setSpacing(14);
    auto* logo = new QLabel;
    QPixmap pm = theme::appIcon(128);
    pm.setDevicePixelRatio(2);
    logo->setPixmap(pm);
    head->addWidget(logo);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    auto* title = new QLabel("APM");
    title->setObjectName("Title");
    auto* sub = new QLabel(QString("Version %1\nApache, PHP and MariaDB for Windows, built from source").arg(apmVersionLong()));
    sub->setTextInteractionFlags(Qt::TextSelectableByMouse);
    sub->setObjectName("Small");
    sub->setWordWrap(true);
    titles->addWidget(title);
    titles->addWidget(sub);
    head->addLayout(titles, 1);
    v->addLayout(head);

    auto* engines = new QLabel(QString("Apache %1   ·   PHP %2   ·   MariaDB %3")
                                   .arg(engine->status(Comp::Apache).version, engine->phpVersion(),
                                        engine->status(Comp::MariaDB).version));
    engines->setObjectName("Muted");
    v->addWidget(engines);

    auto* card = new QFrame;
    card->setObjectName("Card");
    auto* c = new QVBoxLayout(card);
    c->setContentsMargins(16, 12, 16, 12);
    c->setSpacing(4);
    auto* by = new QLabel("Created by");
    by->setObjectName("Section");
    auto* author = new QLabel(credit::kAuthor);
    author->setObjectName("CardTitle");
    const QString link = theme::css(t.accent);
    auto anchor = [&link](const QString& href, const QString& text) {
        auto* l = new QLabel(QString("<a style='color:%1;text-decoration:none' href='%2'>%3</a>").arg(link, href, text));
        l->setTextInteractionFlags(Qt::TextBrowserInteraction);
        l->setOpenExternalLinks(true);
        return l;
    };
    c->addWidget(by);
    c->addWidget(author);
    c->addWidget(anchor(QString("mailto:") + credit::kEmail, credit::kEmail));
    c->addWidget(anchor(credit::kWebsite, QString(credit::kWebsite).remove("https://")));
    c->addWidget(anchor(credit::kRepo, QString(credit::kRepo).remove("https://")));
    v->addWidget(card);

    auto* lic = new QLabel("Apache httpd, PHP, MariaDB, ImageMagick, OpenSSL and Qt keep their own licenses "
                           "(see the licenses folder). APM scripts and this manager: MIT + Commons Clause (selling needs a commercial license).");
    lic->setObjectName("Small");
    lic->setWordWrap(true);
    v->addWidget(lic);

    auto* buttons = new QHBoxLayout;
    auto* licBtn = new QPushButton("Licenses");
    connect(licBtn, &QPushButton::clicked, this, [engine] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(engine->path("licenses")));
    });
    buttons->addWidget(licBtn);
    buttons->addStretch();
    auto* close = new QPushButton("Close");
    close->setObjectName("Primary");
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(close);
    v->addLayout(buttons);
}
