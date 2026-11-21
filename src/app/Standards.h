// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/JointStandard.h"

#include <QString>
#include <QWidget>

#include <optional>
#include <vector>

class QComboBox;
class QLabel;
class QPushButton;

// The joint standards kept on this computer (Documents/Forge3D/Joint Standards.json, beside
// the custom shapes), and the one in use.
namespace forge::app::standardsfile {

QString path();
// The standards kept (none if there's no file yet). Throws std::invalid_argument (in plain
// words) if the file is damaged.
std::vector<standards::Standard> load();
// Throws std::runtime_error if the file can't be written.
void save(const std::vector<standards::Standard>& list);
QString activeName();
void setActiveName(const QString& name);
// The standard in use, if any (a damaged file counts as none).
std::optional<standards::Standard> active();

}  // namespace forge::app::standardsfile

namespace forge::app {

// A row at the top of Quick Socket and Quick Hinge: the joint standard's joints of that
// kind to start from (the label resting on its drop-down), and a button to keep the
// settings as one.
class StandardRow : public QWidget {
    Q_OBJECT

public:
    explicit StandardRow(QWidget* parent = nullptr);
    // The standard's name ("" none in use) and the joints offered.
    void setJoints(const QString& standard, const QStringList& names);

signals:
    // A joint chosen (its place in the list given).
    void chosen(int index);
    void saveRequested();

private:
    QLabel* m_label = nullptr;
    QComboBox* m_joints = nullptr;
    QPushButton* m_save = nullptr;
};

}  // namespace forge::app
