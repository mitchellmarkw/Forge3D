// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace forge::app {

// On Windows, writes a minidump (.dmp) to dumpDirectory if the app crashes,
// so the crash can be diagnosed later. Does nothing on other platforms.
void installCrashHandler(const QString& dumpDirectory);

}  // namespace forge::app
