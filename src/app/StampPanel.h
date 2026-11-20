// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <QStringList>
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace forge::app {

// Surface stamps: which stamp (built in, or one of your custom shapes), its size, height,
// raised or sunk, turn and the spacing of rows, and how many are placed.
class StampPanel : public QWidget {
    Q_OBJECT

public:
    explicit StampPanel(QWidget* parent = nullptr);

    // `custom`: the custom shapes' names. Editing: the stamp kind can't change.
    void start(const QString& title, const QStringList& custom, const StampFinish* editing,
               const QString& editingCustom);
    // The settings (spots left empty). customIndex: which custom shape, or -1.
    StampFinish settings() const;
    int customIndex() const;
    void setCount(int count);
    void setStatus(const QString& text, bool problem);

signals:
    void changed();
    void kindChanged();
    void removeLastRequested();
    void clearRequested();
    void applyRequested();
    void cancelRequested();

private:
    void remember() const;

    QLabel* m_title = nullptr;
    QComboBox* m_kind = nullptr;
    QDoubleSpinBox* m_size = nullptr;
    QDoubleSpinBox* m_height = nullptr;
    QComboBox* m_way = nullptr;
    QDoubleSpinBox* m_spin = nullptr;
    QDoubleSpinBox* m_spacing = nullptr;
    QLabel* m_count = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_apply = nullptr;
    int m_builtIn = 0;  // how many entries are built-in stamps
};

}  // namespace forge::app
