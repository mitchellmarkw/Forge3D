// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Hand.h"

#include <QWidget>

#include <optional>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace forge::app {

class StandardRow;

// Make Hand: fists, gripping, open, relaxed and pointing hands at a figure's scale, left,
// right or a pair, with the socket for the wrist peg and (gripping) a hole for a handle.
class HandPanel : public QWidget {
    Q_OBJECT

public:
    explicit HandPanel(QWidget* parent = nullptr);

    void start();
    HandSettings settings() const;
    bool pair() const;
    // The wrist socket (none: no socket).
    std::optional<SocketFinish> wrist() const;
    // Sizes from a standard's peg or ball joint.
    void setWrist(const SocketFinish& joint);
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);
    StandardRow* standardRow() const { return m_standard; }

protected:
    void resizeEvent(QResizeEvent* event) override;

signals:
    void changed();
    void applyRequested();
    void cancelRequested();

private:
    void refresh();
    void fitHeight();
    void remember() const;

    StandardRow* m_standard = nullptr;
    QComboBox* m_pose = nullptr;
    QComboBox* m_which = nullptr;
    QComboBox* m_scale = nullptr;
    QDoubleSpinBox* m_length = nullptr;
    QDoubleSpinBox* m_fullness = nullptr;
    QWidget* m_gripBox = nullptr;
    QDoubleSpinBox* m_handle = nullptr;
    QDoubleSpinBox* m_gripFit = nullptr;
    QDoubleSpinBox* m_angle = nullptr;
    QComboBox* m_wristKind = nullptr;
    QWidget* m_wristSizes = nullptr;
    QWidget* m_shapeRow = nullptr;
    QComboBox* m_shape = nullptr;
    QLabel* m_sizeLabel = nullptr;
    QDoubleSpinBox* m_size = nullptr;
    QWidget* m_depthRow = nullptr;
    QDoubleSpinBox* m_depth = nullptr;
    QComboBox* m_printer = nullptr;
    QDoubleSpinBox* m_fit = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_apply = nullptr;
};

}  // namespace forge::app
