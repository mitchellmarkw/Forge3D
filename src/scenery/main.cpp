// SPDX-License-Identifier: GPL-3.0-or-later
// Forge3D Scenery: the companion app (scenery for models and dioramas).
#include "BuildInfo.h"
#include "CrashHandler.h"
#include "Log.h"
#include "SceneryWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QTimer>
#include <QtDebug>

namespace {

// An error that slips out of a click is reported and the work kept (as in Forge3D).
class SafeApplication : public QApplication {
public:
    using QApplication::QApplication;

    bool notify(QObject* receiver, QEvent* event) override
    {
        try {
            return QApplication::notify(receiver, event);
        } catch (const std::exception& e) {
            report(QString::fromUtf8(e.what()));
        } catch (...) {
            report(QStringLiteral("unknown error"));
        }
        return false;
    }

private:
    void report(const QString& what)
    {
        qCritical().noquote() << "Unexpected error (caught):" << what;
        QTimer::singleShot(0, this, [this, what] {
            QMessageBox::warning(activeWindow(), QStringLiteral("Forge3D Scenery"),
                                 QApplication::translate("main", "Something went wrong: %1\n\nThat step didn't "
                                                                 "finish, but Forge3D Scenery is still running.")
                                     .arg(what));
        });
    }
};

}  // namespace

int main(int argc, char* argv[])
{
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication::setOrganizationName(QStringLiteral("Forge3D"));
    QApplication::setApplicationName(QString::fromLatin1(forge::build::kAppName));
    QApplication::setApplicationVersion(QString::fromLatin1(forge::build::kVersion));

    SafeApplication app(argc, argv);
    forge::app::theme::apply(app, forge::app::theme::saved());
    forge::app::installLogging();
    forge::app::installCrashHandler(
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/crashes"));
    qInfo().noquote() << "Forge3D Scenery" << forge::build::kVersion << "starting; Qt" << qVersion();

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption screenshot(QStringLiteral("screenshot"),
                                  QStringLiteral("Save a picture of the window to <file> and exit."),
                                  QStringLiteral("file"));
    parser.addOption(screenshot);
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("A .f3d model to open."),
                                 QStringLiteral("[file]"));
    parser.process(app);

    forge::scenery::SceneryWindow window;
    window.show();
    if (!parser.positionalArguments().isEmpty()) {
        const QString path = parser.positionalArguments().constFirst();
        QTimer::singleShot(0, &window, [&window, path] { window.openFile(path); });
    }
    if (parser.isSet(screenshot)) {
        const QString path = parser.value(screenshot);
        QTimer::singleShot(1500, &window, [&window, path] {
            const bool ok = window.grab().save(path);
            QCoreApplication::exit(ok ? 0 : 1);
        });
    }
    return app.exec();
}
