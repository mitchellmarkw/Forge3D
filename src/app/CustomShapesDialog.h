// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QListWidget;
class QPushButton;

namespace forge::app {

// Custom shapes: the saved shapes in the library folder, to add, rename or delete
// (to the Recycle Bin), and where the folder is.
class CustomShapesDialog : public QDialog {
    Q_OBJECT
public:
    // `accessories`: the same for your saved accessories (slice 100).
    CustomShapesDialog(const QString& folder, QWidget* parent = nullptr, bool accessories = false);
    QString folder() const { return m_folder; }

signals:
    void addRequested(const QString& file);
    // A new folder was chosen (the caller keeps it).
    void folderChanged(const QString& folder);

private:
    void reload(const QString& select = {});
    QString currentFile() const;
    void updateButtons();
    void renameCurrent();
    void deleteCurrent();
    void chooseFolder();

    QString m_folder;
    bool m_accessories = false;
    QLabel* m_where = nullptr;
    QListWidget* m_list = nullptr;
    QPushButton* m_add = nullptr;
    QPushButton* m_rename = nullptr;
    QPushButton* m_delete = nullptr;
};

}  // namespace forge::app
