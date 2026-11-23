// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Render.h"

#include <QDialog>

#include <vector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;
class QTimer;

namespace forge {
class Document;
}

namespace forge::app {

// Picture (slice 80): a clean picture of the model for posts or a shop listing (PNG), or a
// turntable GIF of it going once round.
class PictureDialog : public QDialog {
    Q_OBJECT
public:
    PictureDialog(const Document& doc, const std::vector<ObjectId>& ids, const QString& name, QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    bool turntable() const;
    QSize pictureSize() const;
    render::Settings settings(QSize size, int smooth) const;
    void fillSizes();
    void redraw();
    void save();
    void remember() const;

    std::vector<render::Item> m_items;
    QString m_name;
    QComboBox* m_kind = nullptr;
    QComboBox* m_size = nullptr;
    QComboBox* m_background = nullptr;
    QDoubleSpinBox* m_turn = nullptr;
    QDoubleSpinBox* m_tilt = nullptr;
    QSpinBox* m_fill = nullptr;
    QCheckBox* m_shadow = nullptr;
    QComboBox* m_frames = nullptr;
    QDoubleSpinBox* m_seconds = nullptr;
    QWidget* m_gifBox = nullptr;
    QCheckBox* m_spin = nullptr;
    QLabel* m_preview = nullptr;
    QLabel* m_status = nullptr;
    QTimer* m_later = nullptr;
    QTimer* m_spinTimer = nullptr;
    double m_spinAngle = 0.0;
};

}  // namespace forge::app
