// SPDX-License-Identifier: GPL-3.0-or-later
#include "BuildInfo.h"
#include "CrashHandler.h"
#include "Log.h"
#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QTimer>
#include <QSurfaceFormat>
#include <QtDebug>

#include <Standard_Version.hxx>

int main(int argc, char* argv[])
{
    QElapsedTimer startup;
    startup.start();

    // Ask for an OpenGL 3.3 core context with 4x anti-aliasing. This must be set
    // before the QApplication exists so every OpenGL widget gets the same format.
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication::setApplicationName(QString::fromLatin1(forge::build::kAppName));
    QApplication::setApplicationVersion(QString::fromLatin1(forge::build::kVersion));

    QApplication app(argc, argv);

    forge::app::installLogging();
    forge::app::installCrashHandler(
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/crashes"));

    qInfo().noquote() << "Forge3D" << forge::build::kVersion << "starting; Qt" << qVersion()
                      << "| OpenCASCADE" << OCC_VERSION_COMPLETE;
    qInfo().noquote() << "Log file:" << forge::app::logFilePath();
    qInfo() << "Startup: Qt ready after" << startup.elapsed() << "ms";

    // --screenshot <file.png>: render one frame, save the window, exit. Used for automated checks.
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption screenshot(QStringLiteral("screenshot"),
                                  QStringLiteral("Save a picture of the window to <file> and exit."),
                                  QStringLiteral("file"));
    parser.addOption(screenshot);
    QCommandLineOption demo(QStringLiteral("demo-scene"),
                            QStringLiteral("Start with a few sample bodies on the plate."));
    parser.addOption(demo);
    parser.process(app);

    forge::app::MainWindow window;
    qInfo() << "Startup: main window built after" << startup.elapsed() << "ms";
    QObject::connect(&window, &forge::app::MainWindow::firstFrameRendered, [&startup] {
        qInfo() << "Startup: first frame drawn after" << startup.elapsed() << "ms";
    });
    if (parser.isSet(demo))
        window.loadDemoScene();
    window.show();

    if (parser.isSet(screenshot)) {
        const QString path = parser.value(screenshot);
        QTimer::singleShot(1500, &window, [&window, path] {
            const bool ok = window.grab().save(path);
            qInfo().noquote() << (ok ? "Saved screenshot to" : "Could not save screenshot to") << path;
            QCoreApplication::exit(ok ? 0 : 1);
        });
    }

    const int result = app.exec();
    qInfo() << "Exiting with code" << result;
    return result;
}
