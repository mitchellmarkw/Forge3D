// SPDX-License-Identifier: GPL-3.0-or-later
#include "CustomShapesDialog.h"

#include "forge/core/ShapeLibrary.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace forge::app {

namespace {

std::filesystem::path toPath(const QString& s)
{
#ifdef _WIN32
    return std::filesystem::path(s.toStdWString());
#else
    return std::filesystem::path(s.toStdString());
#endif
}

QString fromPath(const std::filesystem::path& p)
{
#ifdef _WIN32
    return QString::fromStdWString(p.wstring());
#else
    return QString::fromStdString(p.string());
#endif
}

}  // namespace

CustomShapesDialog::CustomShapesDialog(const QString& folder, QWidget* parent, bool accessories)
    : QDialog(parent), m_folder(folder), m_accessories(accessories)
{
    setWindowTitle(accessories ? tr("Your Accessories") : tr("Custom Shapes"));
    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(accessories ? tr("Accessories you've saved from Figure Design → Accessories. Each comes "
                                              "in at the size it was saved.")
                                         : tr("Shapes you've saved with Save Custom Shape. Each comes in at the size "
                                              "it was saved. Add one here, or from Add Shape → Custom."));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* row = new QHBoxLayout;
    m_list = new QListWidget;
    m_list->setMinimumSize(260, 260);
    row->addWidget(m_list, 1);
    auto* buttons = new QVBoxLayout;
    m_add = new QPushButton(tr("Add to Model"));
    m_add->setToolTip(tr("Add the picked shape beside the others on the plate"));
    m_rename = new QPushButton(tr("Rename..."));
    m_delete = new QPushButton(tr("Delete"));
    m_delete->setToolTip(tr("Moves the shape's file to the Recycle Bin"));
    auto* open = new QPushButton(tr("Open Folder"));
    open->setToolTip(tr("Show the folder the shapes are kept in (copy shapes in or out, or back them up)"));
    auto* change = new QPushButton(tr("Change Folder..."));
    change->setToolTip(tr("Keep custom shapes in another folder, such as one that's backed up or shared "
                          "between computers"));
    for (QPushButton* b : {m_add, m_rename, m_delete})
        buttons->addWidget(b);
    buttons->addSpacing(12);
    buttons->addWidget(open);
    buttons->addWidget(change);
    buttons->addStretch(1);
    row->addLayout(buttons);
    layout->addLayout(row);

    m_where = new QLabel;
    m_where->setWordWrap(true);
    m_where->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_where->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(m_where);

    auto* close = new QPushButton(tr("Close"));
    auto* bottom = new QHBoxLayout;
    bottom->addStretch(1);
    bottom->addWidget(close);
    layout->addLayout(bottom);

    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_add, &QPushButton::clicked, this, [this] {
        if (const QString file = currentFile(); !file.isEmpty())
            emit addRequested(file);
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this] {
        if (const QString file = currentFile(); !file.isEmpty())
            emit addRequested(file);
    });
    connect(m_rename, &QPushButton::clicked, this, &CustomShapesDialog::renameCurrent);
    connect(m_delete, &QPushButton::clicked, this, &CustomShapesDialog::deleteCurrent);
    connect(open, &QPushButton::clicked, this, [this] {
        QDir().mkpath(m_folder);
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_folder));
    });
    connect(change, &QPushButton::clicked, this, &CustomShapesDialog::chooseFolder);
    connect(m_list, &QListWidget::currentRowChanged, this, &CustomShapesDialog::updateButtons);
    reload();
}

void CustomShapesDialog::reload(const QString& select)
{
    m_list->clear();
    for (const auto& entry : library::list(toPath(m_folder))) {
        auto* item = new QListWidgetItem(QString::fromStdString(entry.name), m_list);
        item->setData(Qt::UserRole, fromPath(entry.file));
        if (!select.isEmpty() && item->text() == select)
            m_list->setCurrentItem(item);
    }
    if (!m_list->currentItem() && m_list->count() > 0)
        m_list->setCurrentRow(0);
    m_where->setText(m_list->count() == 0
                         ? (m_accessories ? tr("None saved yet. Select a shape, then Figure Design → Accessories → "
                                               "Save Selected.\nFolder: %1")
                                          : tr("None saved yet. Select a shape, then Print/Save → Save Custom "
                                               "Shape.\nFolder: %1"))
                               .arg(QDir::toNativeSeparators(m_folder))
                         : tr("Folder: %1").arg(QDir::toNativeSeparators(m_folder)));
    updateButtons();
}

QString CustomShapesDialog::currentFile() const
{
    const QListWidgetItem* item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void CustomShapesDialog::updateButtons()
{
    const bool any = m_list->currentItem() != nullptr;
    for (QPushButton* b : {m_add, m_rename, m_delete})
        b->setEnabled(any);
}

void CustomShapesDialog::renameCurrent()
{
    const QListWidgetItem* item = m_list->currentItem();
    if (!item)
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, m_accessories ? tr("Rename Accessory") : tr("Rename Custom Shape"), tr("New name:"), QLineEdit::Normal,
                                               item->text(), &ok)
                             .trimmed();
    if (!ok || name.isEmpty() || name == item->text())
        return;
    try {
        const auto renamed = library::rename(toPath(currentFile()), name.toStdString());
        const auto stem = renamed.stem().u8string();
        reload(QString::fromStdString(std::string(stem.begin(), stem.end())));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, windowTitle(), QString::fromUtf8(e.what()));
    }
}

void CustomShapesDialog::deleteCurrent()
{
    const QListWidgetItem* item = m_list->currentItem();
    if (!item)
        return;
    const QString name = item->text();
    if (QMessageBox::question(this, windowTitle(),
                              ((m_accessories ? tr("Delete the accessory '%1'?") : tr("Delete the custom shape '%1'?")) +
                               tr("\n\nIts file goes to the Recycle Bin, so it can be got back from there. Shapes "
                                  "already added to models aren't changed."))
                                  .arg(name)) != QMessageBox::Yes)
        return;
    const QString file = currentFile();
    if (!QFile::moveToTrash(file) && !QFile::remove(file)) {
        QMessageBox::warning(this, windowTitle(), tr("'%1' couldn't be deleted.").arg(name));
        return;
    }
    reload();
}

void CustomShapesDialog::chooseFolder()
{
    const QString chosen = QFileDialog::getExistingDirectory(
        this, m_accessories ? tr("Folder for Accessories") : tr("Folder for Custom Shapes"), m_folder);
    if (chosen.isEmpty() || QDir(chosen) == QDir(m_folder))
        return;
    m_folder = chosen;
    emit folderChanged(chosen);
    reload();
}

}  // namespace forge::app
