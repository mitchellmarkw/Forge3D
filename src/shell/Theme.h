// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>

class QApplication;

// Light or dark: the window's colours (Qt's Fusion style with a palette) and the
// colours Forge3D draws with itself (the 3D view, the sketcher, the view cube).
namespace forge::app::theme {

enum class Choice { System, Light, Dark };

Choice saved();               // from Preferences ("ui/theme"); System at first
void save(Choice choice);
// Sets the style and palette for the choice. Call before the main window is made, and
// again when the choice changes (the windows then need repainting).
void apply(QApplication& app, Choice choice);
bool dark();                  // what's in use now

// Colours Forge3D draws with.
struct Look {
    QColor view;              // the 3D view's background
    QColor gridMinor, gridMajor, plateEdge, buildBox;
    QColor sketchPaper, sketchMinor, sketchMajor;
    QColor sketchLine, sketchHeld, sketchHelper, sketchReference, sketchText;
    QColor labelFill, labelRuleFill, labelText;
    QColor cubeFace, cubeEdge, cubeText;
    QColor noteFill, noteText;
};
const Look& look();

}  // namespace forge::app::theme
