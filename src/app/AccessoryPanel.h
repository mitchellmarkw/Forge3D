// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Accessory.h"

#include <QStringList>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;

namespace forge::app {

// Accessories (slice 100): make a blade, shield, staff, belt or holster at the figure's
// scale, or add one of your own saved accessories.
class AccessoryPanel : public QWidget {
    Q_OBJECT
public:
    explicit AccessoryPanel(QWidget* parent = nullptr);

    // `canSave`: a shape was selected, to save as an accessory.
    void start(bool canSave);
    AccessorySettings settings() const;
    void setLibrary(const QStringList& names);
    int libraryIndex() const;
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);

signals:
    void changed();
    void applyRequested();
    void cancelRequested();
    void libraryAddRequested(int index);
    void saveRequested();
    void manageRequested();

private:
    void remember() const;
    void recall();
    void showKind();

    QComboBox* m_kind = nullptr;
    QComboBox* m_scale = nullptr;
    QDoubleSpinBox* m_handle = nullptr;
    QStackedWidget* m_pages = nullptr;
    // Blade
    QDoubleSpinBox *m_bladeLength = nullptr, *m_bladeWidth = nullptr, *m_bladeThickness = nullptr,
                   *m_guardWidth = nullptr, *m_gripLength = nullptr;
    QComboBox* m_tip = nullptr;
    QCheckBox* m_pommel = nullptr;
    // Shield
    QComboBox* m_shieldShape = nullptr;
    QDoubleSpinBox *m_shieldWidth = nullptr, *m_shieldHeight = nullptr, *m_shieldThickness = nullptr,
                   *m_shieldCurve = nullptr;
    QCheckBox *m_boss = nullptr, *m_gripBar = nullptr;
    // Staff
    QDoubleSpinBox* m_staffLength = nullptr;
    QComboBox* m_staffEnd = nullptr;
    // Belt
    QDoubleSpinBox *m_waistWidth = nullptr, *m_waistDepth = nullptr, *m_beltWidth = nullptr, *m_fit = nullptr,
                   *m_gap = nullptr;
    QCheckBox* m_open = nullptr;
    // Holster
    QDoubleSpinBox *m_holsterLength = nullptr, *m_holsterWidth = nullptr, *m_holsterDepth = nullptr;

    QLabel* m_status = nullptr;
    QPushButton* m_apply = nullptr;
    QListWidget* m_library = nullptr;
    QPushButton* m_addOwn = nullptr;
    QPushButton* m_save = nullptr;
    bool m_loading = false;
};

}  // namespace forge::app
