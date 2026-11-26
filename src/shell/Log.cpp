// SPDX-License-Identifier: GPL-3.0-or-later
#include "Log.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>

#include <cstdio>

namespace forge::app {

namespace {
QFile* g_logFile = nullptr;
QMutex g_logMutex;
QString g_logPath;

const char* levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return "DEBUG";
    case QtInfoMsg: return "INFO ";
    case QtWarningMsg: return "WARN ";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg: return "FATAL";
    }
    return "?????";
}

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QString line = QStringLiteral("%1 [%2] %3%4\n")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                  QString::fromLatin1(levelName(type)),
                                  message,
                                  context.file ? QStringLiteral("  (%1:%2)")
                                                     .arg(QString::fromUtf8(context.file))
                                                     .arg(context.line)
                                               : QString());
    const QByteArray utf8 = line.toUtf8();

    QMutexLocker lock(&g_logMutex);
    std::fputs(utf8.constData(), stderr);
    if (g_logFile) {
        g_logFile->write(utf8);
        g_logFile->flush();  // keep the log complete even if we crash next
    }
}
}  // namespace

void installLogging()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/logs");
    QDir().mkpath(dir);
    g_logPath = dir + QStringLiteral("/forge3d.log");

    // Keep the previous session's log for comparison.
    const QString previous = dir + QStringLiteral("/forge3d.previous.log");
    QFile::remove(previous);
    QFile::rename(g_logPath, previous);

    g_logFile = new QFile(g_logPath);
    if (!g_logFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        delete g_logFile;
        g_logFile = nullptr;
        g_logPath.clear();
    }
    qInstallMessageHandler(messageHandler);
}

QString logFilePath()
{
    return g_logPath;
}

}  // namespace forge::app
