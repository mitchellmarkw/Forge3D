// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace forge::app {

// Sends every qDebug/qInfo/qWarning/qCritical message to a log file as well as stderr.
// The file is %LOCALAPPDATA%/Forge3D/logs/forge3d.log on Windows.
// Call once, right after QApplication is created.
void installLogging();

// Full path of the current log file (empty if logging could not start).
QString logFilePath();

}  // namespace forge::app
