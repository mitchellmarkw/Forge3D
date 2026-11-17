// SPDX-License-Identifier: GPL-3.0-or-later
#include "Theme.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>

namespace forge::app::theme {

namespace {

// (The same place MainWindow keeps its preferences.)
class Preferences : public QSettings {
public:
    Preferences() : QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"),
                              QStringLiteral("Forge3D"))
    {
    }
};

bool g_dark = false;
QString g_startStyle;    // the platform's own style and colours, for Light
QPalette g_startPalette;

const Look kLight{
    QColor(237, 240, 245),                                                         // view
    QColor(204, 207, 214), QColor(158, 163, 173), QColor(107, 112, 128), QColor(189, 194, 209),
    QColor(250, 251, 253), QColor(232, 236, 242), QColor(210, 216, 226),           // paper, grid
    QColor(30, 70, 160), QColor(30, 140, 60), QColor(90, 140, 220), QColor(150, 150, 160), QColor(110, 110, 125),
    QColor(255, 255, 255, 235), QColor(240, 243, 250, 235), QColor(40, 40, 60),
    QColor(0xe9, 0xee, 0xf5), QColor(0x5a, 0x63, 0x6e), QColor(0x2b, 0x2f, 0x33),
    QColor(255, 255, 255), QColor(0x2b, 0x2f, 0x33),
};

const Look kDark{
    QColor(38, 41, 47),                                                            // view
    QColor(62, 66, 74), QColor(88, 93, 104), QColor(138, 145, 160), QColor(78, 84, 98),
    QColor(33, 36, 41), QColor(46, 50, 57), QColor(62, 67, 77),                    // paper, grid
    QColor(110, 160, 255), QColor(80, 200, 110), QColor(120, 160, 230), QColor(130, 134, 146), QColor(170, 175, 190),
    QColor(45, 49, 56, 235), QColor(52, 56, 66, 235), QColor(225, 228, 235),
    QColor(70, 76, 88), QColor(150, 158, 170), QColor(230, 232, 236),
    QColor(45, 49, 56), QColor(230, 232, 236),
};

bool systemIsDark()
{
    // Windows: "Choose your app mode". Elsewhere: what the starting palette looks like.
#ifdef Q_OS_WIN
    const QSettings personalise(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
        QSettings::NativeFormat);
    return personalise.value(QStringLiteral("AppsUseLightTheme"), 1).toInt() == 0;
#else
    return QApplication::style()->standardPalette().color(QPalette::Window).lightness() < 128;
#endif
}

QPalette darkPalette()
{
    QPalette p;
    const QColor window(45, 48, 54), base(33, 36, 41), text(225, 228, 235), off(130, 135, 145);
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, QColor(40, 43, 49));
    p.setColor(QPalette::ToolTipBase, QColor(55, 59, 66));
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, off);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, QColor(53, 57, 64));
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, QColor(255, 120, 110));
    p.setColor(QPalette::Light, QColor(70, 75, 84));
    p.setColor(QPalette::Midlight, QColor(60, 64, 72));
    p.setColor(QPalette::Mid, QColor(140, 146, 158));  // used for the grey hints
    p.setColor(QPalette::Dark, QColor(25, 27, 31));
    p.setColor(QPalette::Shadow, QColor(10, 10, 12));
    p.setColor(QPalette::Highlight, QColor(43, 120, 208));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, QColor(110, 170, 255));
    p.setColor(QPalette::LinkVisited, QColor(170, 140, 255));
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, QColor(105, 110, 120));
    p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(70, 75, 84));
    p.setColor(QPalette::Disabled, QPalette::Base, window);
    return p;
}

}  // namespace

Choice saved()
{
    const QString v = Preferences().value(QStringLiteral("ui/theme")).toString();
    if (v == QLatin1String("light"))
        return Choice::Light;
    if (v == QLatin1String("dark"))
        return Choice::Dark;
    return Choice::System;
}

void save(Choice choice)
{
    Preferences().setValue(QStringLiteral("ui/theme"), choice == Choice::Light  ? QStringLiteral("light")
                                                       : choice == Choice::Dark ? QStringLiteral("dark")
                                                                                : QStringLiteral("system"));
}

void apply(QApplication& app, Choice choice)
{
    if (g_startStyle.isEmpty()) {
        g_startStyle = QApplication::style()->name();
        g_startPalette = QApplication::palette();
    }
    g_dark = choice == Choice::Dark || (choice == Choice::System && systemIsDark());
    if (g_dark) {
        // Fusion takes a palette in full (the Windows styles draw some parts themselves).
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        QApplication::setPalette(darkPalette());
    } else if (g_startPalette.color(QPalette::Window).lightness() >= 128) {
        QApplication::setStyle(QStyleFactory::create(g_startStyle));
        QApplication::setPalette(g_startPalette);
    } else {
        // The computer started us dark: Fusion's own light colours.
        QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion"));
        QApplication::setStyle(fusion);
        QApplication::setPalette(fusion->standardPalette());
    }
    (void)app;
}

bool dark()
{
    return g_dark;
}

const Look& look()
{
    return g_dark ? kDark : kLight;
}

}  // namespace forge::app::theme
