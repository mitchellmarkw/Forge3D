// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/PrintCheck.h"

#include <QColor>
#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;

namespace forge::app {

// Print Check: what to look for (thin walls, overhangs), the colours they're shown in,
// and how much of each was found. Nothing here changes the parts.
class PrintCheckPanel : public QWidget {
    Q_OBJECT

public:
    explicit PrintCheckPanel(QWidget* parent = nullptr);

    printcheck::Settings settings() const;
    // The colours: fine, overhanging, too thin.
    static QColor okColour() { return QColor(222, 222, 222); }
    static QColor overhangColour() { return QColor(40, 120, 255); }
    static QColor thinColour() { return QColor(235, 35, 25); }

    void setResult(const QString& html);
    void setStatus(const QString& text, bool problem);

signals:
    void settingsChanged();
    void doneRequested();

private:
    void remember();

    QCheckBox* m_thin = nullptr;
    QDoubleSpinBox* m_minWall = nullptr;
    QCheckBox* m_overhangs = nullptr;
    QDoubleSpinBox* m_angle = nullptr;
    QLabel* m_result = nullptr;
    QLabel* m_status = nullptr;
};

}  // namespace forge::app
