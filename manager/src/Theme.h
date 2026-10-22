// Theme.h - colour tokens (light/dark, follows Windows), stylesheet, Material icons, app icon.
#pragma once
#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

class QApplication;

struct Tokens {
    bool dark = false;
    QColor bg, surface, surface2, border, text, muted, accent, accentHover, onAccent, ok, warn, bad, idle;
};

namespace theme {
const Tokens& t();
void apply(QApplication& app);          // palette + stylesheet for the current Windows colour scheme
QIcon icon(const QString& name, const QColor& color, int size = 20);
QPixmap appIcon(int size, const QColor& dot = QColor());
QString css(const QColor& c);           // "#rrggbb"
} // namespace theme
