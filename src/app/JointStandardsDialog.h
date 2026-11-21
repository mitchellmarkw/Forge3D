// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/JointStandard.h"

#include <QDialog>

#include <vector>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTreeWidget;

namespace forge::app {

// Tools → Joint Standards: a figure line's joint sizes, kept by name. Choose the one in
// use, make, rename, remove, import or export them, and tidy their joints (joints are
// added from Quick Socket and Quick Hinge with Save to Standard).
class JointStandardsDialog : public QDialog {
    Q_OBJECT

public:
    explicit JointStandardsDialog(QWidget* parent = nullptr);
    // Reads the file again (after a joint was saved from a panel).
    void reload();

signals:
    // The standards or the one in use changed.
    void changed();

private:
    void showStandard();
    void store();
    int current() const;
    void setStatus(const QString& text, bool problem);

    std::vector<standards::Standard> m_list;
    bool m_loading = false;
    QComboBox* m_standard = nullptr;
    QPlainTextEdit* m_notes = nullptr;
    QTreeWidget* m_joints = nullptr;
    QWidget* m_body = nullptr;
    QLabel* m_status = nullptr;
};

}  // namespace forge::app
