#include "Theme.h"

#include <QApplication>
#include <QFont>
#include <QHash>
#include <QLinearGradient>
#include <QLocale>
#include <QPalette>
#include <QPainter>
#include <QPainterPath>
#include <QStyleFactory>
#include <QStyleHints>
#include <QSvgRenderer>

namespace {
Tokens g;

// Material Symbols (Apache License 2.0), 24x24 paths
const QHash<QString, QString>& paths()
{
    static const QHash<QString, QString> p = {
        {"play", "M8 5v14l11-7z"},
        {"stop", "M6 6h12v12H6z"},
        {"restart", "M17.65 6.35A7.958 7.958 0 0 0 12 4c-4.42 0-7.99 3.58-7.99 8s3.57 8 7.99 8c3.73 0 6.84-2.55 7.73-6h-2.08A5.99 5.99 0 0 1 12 18c-3.31 0-6-2.69-6-6s2.69-6 6-6c1.66 0 3.14.69 4.22 1.78L13 11h7V4l-2.35 2.35z"},
        {"log", "M14 2H6c-1.1 0-1.99.9-1.99 2L4 20c0 1.1.89 2 1.99 2H18c1.1 0 2-.9 2-2V8l-6-6zm2 16H8v-2h8v2zm0-4H8v-2h8v2zm-3-5V3.5L18.5 9H13z"},
        {"tune", "M3 17v2h6v-2H3zM3 5v2h10V5H3zm10 16v-2h8v-2h-8v-2h-2v6h2zM7 9v2H3v2h4v2h2V9H7zm14 4v-2H11v2h10zm-6-4h2V7h4V5h-4V3h-2v6z"},
        {"folder", "M10 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z"},
        {"open", "M19 19H5V5h7V3H5a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14c1.1 0 2-.9 2-2v-7h-2v7zM14 3v2h3.59l-9.83 9.83 1.41 1.41L19 6.41V10h2V3h-7z"},
        {"settings", "M19.14 12.94c.04-.3.06-.61.06-.94 0-.32-.02-.64-.07-.94l2.03-1.58a.49.49 0 0 0 .12-.61l-1.92-3.32a.488.488 0 0 0-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54a.484.484 0 0 0-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.05.3-.09.63-.09.94s.02.64.07.94l-2.03 1.58a.49.49 0 0 0-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z"},
        {"update", "M5 20h14v-2H5v2zM19 9h-4V3H9v6H5l7 7 7-7z"},
        {"power", "M13 3h-2v10h2V3zm4.83 2.17l-1.42 1.42A6.92 6.92 0 0 1 19 12c0 3.87-3.13 7-7 7A6.995 6.995 0 0 1 7.58 6.58L6.17 5.17A8.932 8.932 0 0 0 3 12a9 9 0 0 0 18 0c0-2.74-1.23-5.18-3.17-6.83z"},
        {"globe", "M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm6.93 6h-2.95a15.65 15.65 0 0 0-1.38-3.56A8.03 8.03 0 0 1 18.93 8zM12 4.04c.83 1.2 1.48 2.53 1.91 3.96h-3.82c.43-1.43 1.08-2.76 1.91-3.96zM4.26 14C4.1 13.36 4 12.69 4 12s.1-1.36.26-2h3.38c-.08.66-.14 1.32-.14 2s.06 1.34.14 2H4.26zm.82 2h2.95c.32 1.25.78 2.45 1.38 3.56A7.987 7.987 0 0 1 5.08 16zm2.95-8H5.08a7.987 7.987 0 0 1 4.33-3.56A15.65 15.65 0 0 0 8.03 8zM12 19.96c-.83-1.2-1.48-2.53-1.91-3.96h3.82c-.43 1.43-1.08 2.76-1.91 3.96zM14.34 14H9.66c-.09-.66-.16-1.32-.16-2s.07-1.35.16-2h4.68c.09.65.16 1.32.16 2s-.07 1.34-.16 2zm.25 5.56c.6-1.11 1.06-2.31 1.38-3.56h2.95a8.03 8.03 0 0 1-4.33 3.56zM16.36 14c.08-.66.14-1.32.14-2s-.06-1.34-.14-2h3.38c.16.64.26 1.31.26 2s-.1 1.36-.26 2h-3.38z"},
        {"terminal", "M20 4H4c-1.11 0-2 .9-2 2v12c0 1.1.89 2 2 2h16c1.1 0 2-.9 2-2V6c0-1.1-.9-2-2-2zm0 14H4V8h16v10zm-2-1h-6v-2h6v2zM7.5 17l-1.41-1.41L8.67 13l-2.59-2.59L7.5 9l4 4-4 4z"},
        {"shield", "M12 1 3 5v6c0 5.55 3.84 10.74 9 12 5.16-1.26 9-6.45 9-12V5l-9-4zm-2 16-4-4 1.41-1.41L10 14.17l6.59-6.59L18 9l-8 8z"},
        {"close", "M19 6.41 17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z"},
        {"check", "M9 16.17 4.83 12l-1.42 1.41L9 19 21 7l-1.41-1.41z"},
    };
    return p;
}

void pickTokens(bool dark)
{
    g.dark = dark;
    if (dark) {
        g.bg = QColor("#15171c");       g.surface = QColor("#1e2128");  g.surface2 = QColor("#272b33");
        g.border = QColor("#30353f");   g.text = QColor("#e8eaee");     g.muted = QColor("#9aa2ae");
        g.accent = QColor("#5b8cff");   g.accentHover = QColor("#7aa2ff"); g.onAccent = QColor("#0b1220");
        g.ok = QColor("#34d399");       g.warn = QColor("#fbbf24");     g.bad = QColor("#f87171");  g.idle = QColor("#6b7280");
    } else {
        g.bg = QColor("#f4f5f8");       g.surface = QColor("#ffffff");  g.surface2 = QColor("#eef0f4");
        g.border = QColor("#e1e4ea");   g.text = QColor("#1b1e24");     g.muted = QColor("#667085");
        g.accent = QColor("#2f6bed");   g.accentHover = QColor("#2459cc"); g.onAccent = QColor("#ffffff");
        g.ok = QColor("#12a150");       g.warn = QColor("#d97706");     g.bad = QColor("#dc2626");  g.idle = QColor("#98a2b3");
    }
}
} // namespace

namespace theme {

const Tokens& t() { return g; }

QString css(const QColor& c) { return c.name(QColor::HexRgb); }

void apply(QApplication& app)
{
    // APM_THEME=light|dark overrides the Windows setting (screenshots, testing)
    const QString forced = qEnvironmentVariable("APM_THEME");
    pickTokens(forced.isEmpty() ? QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark : forced == "dark");
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette pal;
    pal.setColor(QPalette::Window, g.bg);
    pal.setColor(QPalette::Base, g.surface);
    pal.setColor(QPalette::AlternateBase, g.surface2);
    pal.setColor(QPalette::Text, g.text);
    pal.setColor(QPalette::WindowText, g.text);
    pal.setColor(QPalette::Button, g.surface2);
    pal.setColor(QPalette::ButtonText, g.text);
    pal.setColor(QPalette::Highlight, g.accent);
    pal.setColor(QPalette::HighlightedText, g.onAccent);
    pal.setColor(QPalette::PlaceholderText, g.muted);
    pal.setColor(QPalette::ToolTipBase, g.surface);
    pal.setColor(QPalette::ToolTipText, g.text);
    app.setPalette(pal);

    // numbers (ports, versions) in Western digits even on a Thai-locale Windows
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));

    QFont f("Segoe UI Variable Text");
    if (!f.exactMatch()) f = QFont("Segoe UI");
    f.setPointSizeF(9.5);
    app.setFont(f);

    const QString s = QString(R"(
        QWidget#Root, QDialog { background: %1; }
        QFrame#Card { background: %2; border: 1px solid %3; border-radius: 12px; }
        QLabel { color: %4; }
        QLabel#Title { font-size: 18pt; font-weight: 600; }
        QLabel#CardTitle { font-size: 11.5pt; font-weight: 600; }
        QLabel#Muted, QLabel#Small { color: %5; }
        QLabel#Small { font-size: 8.5pt; }
        QLabel#Section { color: %5; font-size: 8.5pt; font-weight: 600; letter-spacing: 1px; }
        QLabel#Message { color: %6; font-size: 8.5pt; }
        QPushButton { background: %7; color: %4; border: 1px solid %3; border-radius: 8px; padding: 7px 14px; }
        QPushButton:hover { border-color: %8; }
        QPushButton:disabled { color: %5; }
        QPushButton#Primary { background: %8; color: %9; border: none; font-weight: 600; }
        QPushButton#Primary:hover { background: %10; }
        QPushButton#Primary:disabled { background: %3; color: %5; }
        QToolButton { background: transparent; border: none; border-radius: 8px; padding: 6px; }
        QToolButton:hover { background: %7; }
        QToolButton:disabled { background: transparent; }
        QToolButton#Link { color: %5; padding: 6px 8px; }
        QToolButton#Link:hover { color: %4; }
        QLineEdit, QSpinBox, QPlainTextEdit, QComboBox { background: %2; color: %4; border: 1px solid %3; border-radius: 6px; padding: 5px 7px; }
        QLineEdit:focus, QSpinBox:focus { border-color: %8; }
        QPlainTextEdit { font-family: "Cascadia Mono", Consolas, monospace; font-size: 8.5pt; }
        QCheckBox { color: %4; spacing: 8px; }
        QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px; border: 1.5px solid %5; background: %2; }
        QCheckBox::indicator:hover { border-color: %8; }
        QCheckBox::indicator:checked { background: %8; border-color: %8; image: url(:/check.svg); }
        QCheckBox::indicator:disabled { border-color: %3; }
        QCheckBox::indicator:checked:disabled { background: %3; }
        QCheckBox:disabled { color: %5; }
        QToolTip { background: %2; color: %4; border: 1px solid %3; padding: 4px 6px; }
        QMenu { background: %2; color: %4; border: 1px solid %3; padding: 4px; }
        QMenu::item { padding: 6px 22px 6px 10px; border-radius: 6px; }
        QMenu::item:selected { background: %7; }
        QMenu::separator { height: 1px; background: %3; margin: 4px 6px; }
        QScrollArea { border: none; background: transparent; }
        QProgressBar { background: %7; border: 1px solid %3; border-radius: 6px; height: 16px; text-align: center; color: %4; }
        QProgressBar::chunk { background: %8; border-radius: 5px; }
        QTableWidget { background: %2; border: 1px solid %3; border-radius: 8px; gridline-color: %3; }
        QHeaderView::section { background: %7; color: %5; border: none; padding: 5px; }
    )")
        .arg(css(g.bg), css(g.surface), css(g.border), css(g.text), css(g.muted), css(g.warn), css(g.surface2),
             css(g.accent), css(g.onAccent))
        .arg(css(g.accentHover));
    app.setStyleSheet(s);
}

QIcon icon(const QString& name, const QColor& color, int size)
{
    const QString d = paths().value(name);
    const QByteArray svg = QString(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path fill="%1" d="%2"/></svg>)")
                               .arg(css(color), d).toUtf8();
    QSvgRenderer r(svg);
    QIcon ic;
    for (qreal dpr : {1.0, 1.5, 2.0}) {
        QPixmap pm(QSize(size, size) * dpr);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        r.render(&p);
        p.end();
        pm.setDevicePixelRatio(dpr);
        ic.addPixmap(pm);
    }
    return ic;
}

QPixmap appIcon(int size, const QColor& dot)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r(0.5, 0.5, size - 1, size - 1);
    QLinearGradient grad(r.topLeft(), r.bottomRight());
    grad.setColorAt(0, QColor("#3b7bff"));
    grad.setColorAt(1, QColor("#6d4dff"));
    QPainterPath path;
    path.addRoundedRect(r, size * 0.24, size * 0.24);
    p.fillPath(path, grad);

    // stylised "A": two strokes + bar
    QPen pen(Qt::white, size * 0.11, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    const qreal s = size;
    QPainterPath a;
    a.moveTo(s * 0.27, s * 0.76);
    a.lineTo(s * 0.50, s * 0.24);
    a.lineTo(s * 0.73, s * 0.76);
    p.drawPath(a);
    p.drawLine(QPointF(s * 0.37, s * 0.56), QPointF(s * 0.63, s * 0.56));

    if (dot.isValid()) {
        const qreal d = s * 0.42;
        const QRectF dr(s - d - 0.5, s - d - 0.5, d, d);
        p.setPen(QPen(QColor("#ffffff"), s * 0.07));
        p.setBrush(dot);
        p.drawEllipse(dr);
    }
    return pm;
}

} // namespace theme
