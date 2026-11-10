// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"

#include "BuildInfo.h"
#include "Log.h"
#include "Viewport.h"

#include "forge/core/Primitives.h"
#include "forge/core/Tessellator.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QUrl>
#include <QtDebug>

#include <Standard_Version.hxx>

#include <exception>

namespace forge::app {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("%1 %2").arg(QString::fromLatin1(build::kAppName),
                                               QString::fromLatin1(build::kVersion)));
    resize(1280, 800);

    m_viewport = new Viewport(this);
    setCentralWidget(m_viewport);

    createMenus();
    createStatusBar();
    loadStarterModel();
}

void MainWindow::createMenus()
{
    QMenu* file = menuBar()->addMenu(tr("&File"));
    QAction* quit = file->addAction(tr("E&xit"), this, &QWidget::close);
    quit->setShortcut(QKeySequence::Quit);

    QMenu* view = menuBar()->addMenu(tr("&View"));
    QAction* fit = view->addAction(tr("&Fit model"), m_viewport, &Viewport::fitView);
    fit->setShortcut(Qt::Key_F);
    QAction* reset = view->addAction(tr("&Reset view"), m_viewport, &Viewport::resetView);
    reset->setShortcut(Qt::Key_Home);

    QMenu* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("Open &log folder"), this, [] {
        const QString path = logFilePath();
        if (!path.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    });
    help->addSeparator();
    help->addAction(tr("&About Forge3D"), this, &MainWindow::showAbout);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(
        tr("Right-drag: orbit   |   Middle-drag or Shift+right-drag: pan   |   Wheel: zoom   |   "
           "F: fit   |   Home: reset view"));

    m_modelInfo = new QLabel(this);
    m_glInfo = new QLabel(this);
    statusBar()->addPermanentWidget(m_modelInfo);
    statusBar()->addPermanentWidget(m_glInfo);

    connect(m_viewport, &Viewport::glInfoReady, this, [this](const QString& summary) {
        m_glInfo->setText(m_viewport->glVersionString());
        m_glInfo->setToolTip(summary);
    });
}

void MainWindow::loadStarterModel()
{
    // Slice 1 shows a fixed 40 x 30 x 20 mm box so the pipeline can be checked end to end:
    // OpenCASCADE builds an exact solid, the tessellator turns it into triangles,
    // and the viewport draws them. Real model creation arrives in slice 3.
    try {
        const TopoDS_Shape box = forge::primitives::makeBox(40.0, 30.0, 20.0);
        const forge::Mesh mesh = forge::tessellate(box);
        m_viewport->setMesh(mesh);
        m_viewport->resetView();
        m_modelInfo->setText(tr("Box 40 x 30 x 20 mm  |  %1 triangles").arg(mesh.triangleCount()));
        qInfo() << "Starter model:" << mesh.triangleCount() << "triangles," << mesh.vertexCount()
                << "vertices";
    } catch (const std::exception& e) {
        qCritical() << "Could not build the starter model:" << e.what();
        QMessageBox::critical(this, tr("Forge3D"),
                              tr("Could not build the starter model:\n%1").arg(QString::fromUtf8(e.what())));
    }
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, tr("About Forge3D"),
        tr("<h3>Forge3D %1</h3>"
           "<p>Parametric 3D modeling with sculpting.</p>"
           "<p>Qt %2 &middot; OpenCASCADE %3 &middot; %4</p>"
           "<p>Copyright &copy; 2026 the Forge3D contributors.<br>"
           "This program is free software: you can redistribute it and/or modify it under the terms "
           "of the GNU General Public License, version 3 or any later version. It comes with "
           "ABSOLUTELY NO WARRANTY.</p>")
            .arg(QString::fromLatin1(build::kVersion), QString::fromLatin1(qVersion()),
                 QString::fromLatin1(OCC_VERSION_COMPLETE), m_viewport->glVersionString()));
}

}  // namespace forge::app
