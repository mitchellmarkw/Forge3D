// SPDX-License-Identifier: GPL-3.0-or-later
#include "AccessoryPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace forge::app {

namespace {

QSettings prefs()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"),
                     QStringLiteral("Forge3D"));
}

QDoubleSpinBox* spin(double low, double high, double step, const QString& suffix, int decimals = 1)
{
    auto* box = new QDoubleSpinBox;
    box->setRange(low, high);
    box->setDecimals(decimals);
    box->setSingleStep(step);
    box->setSuffix(suffix);
    box->setKeyboardTracking(false);
    return box;
}

// A combo box with its label right on top of it.
QWidget* labelled(const QString& text, QComboBox* box)
{
    auto* holder = new QWidget;
    auto* column = new QVBoxLayout(holder);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(new QLabel(text));
    column->addWidget(box);
    return holder;
}

// Figure scales: 1 to this.
constexpr double kScales[] = {6.0, 10.0, 12.0, 18.0};

}  // namespace

AccessoryPanel::AccessoryPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel(tr("Accessories"));
    QFont bold = title->font();
    bold.setBold(true);
    title->setFont(bold);
    layout->addWidget(title);
    auto* intro = new QLabel(tr("Make one at your figure's scale (sizes are life size, as for a person), its "
                                "handle sized to the hands' grip holes. Or add one of your own below."));
    intro->setWordWrap(true);
    intro->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(intro);

    auto* makeBox = new QGroupBox(tr("Make one"));
    auto* make = new QVBoxLayout(makeBox);
    m_kind = new QComboBox;
    for (int k = 0; k < kAccessoryKindCount; ++k)
        m_kind->addItem(tr(accessory::title(AccessoryKind(k))));
    make->addWidget(labelled(tr("What to make:"), m_kind));
    m_scale = new QComboBox;
    m_scale->addItem(tr("1:6 (12-inch figures)"));
    m_scale->addItem(tr("1:10 (7-inch figures)"));
    m_scale->addItem(tr("1:12 (6-inch figures)"));
    m_scale->addItem(tr("1:18 (3.75-inch figures)"));
    m_scale->setToolTip(tr("The figure's scale: the life sizes below are made this much smaller"));
    make->addWidget(labelled(tr("Scale:"), m_scale));
    auto* handleForm = new QFormLayout;
    m_handle = spin(0.3, 40.0, 0.1, tr(" mm"), 2);
    m_handle->setToolTip(tr("As printed: the diameter of the grip, staff or shield bar. The hands' grip holes "
                            "(Make Hand, Hand Ports) take this size."));
    handleForm->addRow(tr("Handle:"), m_handle);
    make->addLayout(handleForm);

    m_pages = new QStackedWidget;
    const QString cm = tr(" cm");
    {  // Blade
        auto* page = new QWidget;
        auto* col = new QVBoxLayout(page);
        col->setContentsMargins(0, 0, 0, 0);
        auto* form = new QFormLayout;
        m_bladeLength = spin(2.0, 500.0, 5.0, cm);
        m_bladeLength->setToolTip(tr("Life size, guard to tip: a dagger 20-35 cm, a sword 60-100 cm"));
        form->addRow(tr("Blade length:"), m_bladeLength);
        m_bladeWidth = spin(0.5, 100.0, 0.5, cm);
        form->addRow(tr("Blade width:"), m_bladeWidth);
        m_bladeThickness = spin(0.05, 20.0, 0.1, cm, 2);
        m_bladeThickness->setToolTip(tr("Life size; never printed thinner than 0.8 mm"));
        form->addRow(tr("Blade thickness:"), m_bladeThickness);
        m_guardWidth = spin(0.0, 200.0, 1.0, cm);
        form->addRow(tr("Guard width:"), m_guardWidth);
        m_gripLength = spin(1.0, 200.0, 1.0, cm);
        form->addRow(tr("Grip length:"), m_gripLength);
        col->addLayout(form);
        m_tip = new QComboBox;
        m_tip->addItem(tr("Pointed"));
        m_tip->addItem(tr("Rounded"));
        m_tip->addItem(tr("Angled (tanto)"));
        col->addWidget(labelled(tr("Tip:"), m_tip));
        m_pommel = new QCheckBox(tr("Pommel on the end"));
        col->addWidget(m_pommel);
        col->addStretch(1);  // the rows stay together at the top
        m_pages->addWidget(page);
    }
    {  // Shield
        auto* page = new QWidget;
        auto* col = new QVBoxLayout(page);
        col->setContentsMargins(0, 0, 0, 0);
        m_shieldShape = new QComboBox;
        m_shieldShape->addItem(tr("Round"));
        m_shieldShape->addItem(tr("Heater (flat top, pointed foot)"));
        m_shieldShape->addItem(tr("Kite (round top, long point)"));
        col->addWidget(labelled(tr("Shape:"), m_shieldShape));
        auto* form = new QFormLayout;
        m_shieldWidth = spin(5.0, 500.0, 5.0, cm);
        form->addRow(tr("Width:"), m_shieldWidth);
        m_shieldHeight = spin(5.0, 500.0, 5.0, cm);
        form->addRow(tr("Height:"), m_shieldHeight);
        m_shieldThickness = spin(0.1, 20.0, 0.5, cm);
        m_shieldThickness->setToolTip(tr("Life size; never printed thinner than 1 mm"));
        form->addRow(tr("Thickness:"), m_shieldThickness);
        m_shieldCurve = spin(0.0, 100.0, 10.0, tr(" %"), 0);
        m_shieldCurve->setToolTip(tr("How much it bows round the arm: 0 is flat"));
        form->addRow(tr("Curve:"), m_shieldCurve);
        col->addLayout(form);
        m_boss = new QCheckBox(tr("Boss in the middle"));
        col->addWidget(m_boss);
        m_gripBar = new QCheckBox(tr("Grip bar on the back (for a gripping hand)"));
        col->addWidget(m_gripBar);
        col->addStretch(1);  // the rows stay together at the top
        m_pages->addWidget(page);
    }
    {  // Staff
        auto* page = new QWidget;
        auto* col = new QVBoxLayout(page);
        col->setContentsMargins(0, 0, 0, 0);
        auto* form = new QFormLayout;
        m_staffLength = spin(10.0, 1000.0, 10.0, cm);
        form->addRow(tr("Length:"), m_staffLength);
        col->addLayout(form);
        m_staffEnd = new QComboBox;
        m_staffEnd->addItem(tr("Plain (rounded ends)"));
        m_staffEnd->addItem(tr("Spear point"));
        m_staffEnd->addItem(tr("Orb on top"));
        col->addWidget(labelled(tr("Top end:"), m_staffEnd));
        col->addStretch(1);  // the rows stay together at the top
        m_pages->addWidget(page);
    }
    {  // Belt
        auto* page = new QWidget;
        auto* col = new QVBoxLayout(page);
        col->setContentsMargins(0, 0, 0, 0);
        auto* note = new QLabel(tr("Round the figure's waist, measured as printed (Measure, or the waist's size in "
                                   "Properties). For a belt that hugs an uneven body, use Make Part."));
        note->setWordWrap(true);
        note->setStyleSheet(QStringLiteral("color: palette(mid);"));
        col->addWidget(note);
        auto* form = new QFormLayout;
        m_waistWidth = spin(2.0, 1000.0, 0.5, tr(" mm"));
        form->addRow(tr("Waist across:"), m_waistWidth);
        m_waistDepth = spin(2.0, 1000.0, 0.5, tr(" mm"));
        form->addRow(tr("Waist front to back:"), m_waistDepth);
        m_beltWidth = spin(0.5, 50.0, 0.5, cm);
        m_beltWidth->setToolTip(tr("Life size, up and down (a holster's loop takes this belt)"));
        form->addRow(tr("Belt width:"), m_beltWidth);
        m_fit = spin(0.0, 2.0, 0.05, tr(" mm"), 2);
        m_fit->setToolTip(tr("Room all round the waist"));
        form->addRow(tr("Fit:"), m_fit);
        col->addLayout(form);
        m_open = new QCheckBox(tr("Open at the back, to clip it on"));
        col->addWidget(m_open);
        auto* gapForm = new QFormLayout;
        m_gap = spin(0.0, 100.0, 0.5, tr(" mm"));
        gapForm->addRow(tr("Gap:"), m_gap);
        col->addLayout(gapForm);
        col->addStretch(1);  // the rows stay together at the top
        m_pages->addWidget(page);
    }
    {  // Holster
        auto* page = new QWidget;
        auto* col = new QVBoxLayout(page);
        col->setContentsMargins(0, 0, 0, 0);
        auto* form = new QFormLayout;
        m_holsterLength = spin(2.0, 200.0, 1.0, cm);
        form->addRow(tr("Length:"), m_holsterLength);
        m_holsterWidth = spin(1.0, 100.0, 0.5, cm);
        form->addRow(tr("Width:"), m_holsterWidth);
        m_holsterDepth = spin(1.0, 100.0, 0.5, cm);
        form->addRow(tr("Depth:"), m_holsterDepth);
        col->addLayout(form);
        auto* note = new QLabel(tr("The loop on the back takes the belt set under Belt with buckle."));
        note->setWordWrap(true);
        note->setStyleSheet(QStringLiteral("color: palette(mid);"));
        col->addWidget(note);
        col->addStretch(1);  // the rows stay together at the top
        m_pages->addWidget(page);
    }
    make->addWidget(m_pages);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    make->addWidget(m_status);
    auto* buttons = new QHBoxLayout;
    m_apply = new QPushButton(tr("Make"));
    m_apply->setDefault(true);
    auto* cancel = new QPushButton(tr("Close"));
    buttons->addWidget(m_apply);
    buttons->addWidget(cancel);
    make->addLayout(buttons);
    layout->addWidget(makeBox);

    auto* ownBox = new QGroupBox(tr("Your accessories"));
    auto* own = new QVBoxLayout(ownBox);
    m_library = new QListWidget;
    m_library->setMaximumHeight(140);
    m_library->setToolTip(tr("Accessories you've saved: added at the size they were saved"));
    own->addWidget(m_library);
    auto* ownButtons = new QHBoxLayout;
    m_addOwn = new QPushButton(tr("Add"));
    m_save = new QPushButton(tr("Save Selected..."));
    m_save->setToolTip(tr("Save the shape that was selected when you opened Accessories as one of yours"));
    auto* manage = new QPushButton(tr("Manage..."));
    manage->setToolTip(tr("Rename or delete your accessories, or choose the folder they're kept in"));
    ownButtons->addWidget(m_addOwn);
    ownButtons->addWidget(m_save);
    ownButtons->addWidget(manage);
    own->addLayout(ownButtons);
    layout->addWidget(ownBox);
    layout->addStretch(1);

    connect(m_kind, &QComboBox::currentIndexChanged, this, [this] { showKind(); });
    for (QComboBox* c : {m_kind, m_scale, m_tip, m_shieldShape, m_staffEnd})
        connect(c, &QComboBox::currentIndexChanged, this, [this] {
            if (!m_loading) {
                remember();
                emit changed();
            }
        });
    for (QDoubleSpinBox* s : {m_handle, m_bladeLength, m_bladeWidth, m_bladeThickness, m_guardWidth, m_gripLength,
                              m_shieldWidth, m_shieldHeight, m_shieldThickness, m_shieldCurve, m_staffLength,
                              m_waistWidth, m_waistDepth, m_beltWidth, m_fit, m_gap, m_holsterLength,
                              m_holsterWidth, m_holsterDepth})
        connect(s, &QDoubleSpinBox::valueChanged, this, [this] {
            if (!m_loading) {
                remember();
                emit changed();
            }
        });
    for (QCheckBox* c : {m_pommel, m_boss, m_gripBar, m_open})
        connect(c, &QCheckBox::toggled, this, [this] {
            m_gap->setEnabled(m_open->isChecked());
            if (!m_loading) {
                remember();
                emit changed();
            }
        });
    connect(m_shieldShape, &QComboBox::currentIndexChanged, this,
            [this](int i) { m_shieldHeight->setEnabled(i != 0); });
    connect(m_apply, &QPushButton::clicked, this, &AccessoryPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &AccessoryPanel::cancelRequested);
    connect(m_addOwn, &QPushButton::clicked, this, [this] {
        if (m_library->currentRow() >= 0)
            emit libraryAddRequested(m_library->currentRow());
    });
    connect(m_library, &QListWidget::itemDoubleClicked, this, [this] {
        if (m_library->currentRow() >= 0)
            emit libraryAddRequested(m_library->currentRow());
    });
    connect(m_library, &QListWidget::currentRowChanged, this, [this](int row) { m_addOwn->setEnabled(row >= 0); });
    connect(m_save, &QPushButton::clicked, this, &AccessoryPanel::saveRequested);
    connect(manage, &QPushButton::clicked, this, &AccessoryPanel::manageRequested);
    recall();
}

void AccessoryPanel::showKind()
{
    m_pages->setCurrentIndex(m_kind->currentIndex());
}

void AccessoryPanel::start(bool canSave)
{
    recall();
    m_save->setEnabled(canSave);
    m_save->setToolTip(canSave ? tr("Save the shape that was selected when you opened Accessories as one of yours")
                               : tr("Select a shape first, then open Accessories, to save it as one of yours"));
}

AccessorySettings AccessoryPanel::settings() const
{
    AccessorySettings s;
    s.kind = AccessoryKind(std::clamp(m_kind->currentIndex(), 0, kAccessoryKindCount - 1));
    s.scale = kScales[std::clamp(m_scale->currentIndex(), 0, 3)];
    s.handle = m_handle->value();
    s.bladeLength = m_bladeLength->value() * 10.0;
    s.bladeWidth = m_bladeWidth->value() * 10.0;
    s.bladeThickness = m_bladeThickness->value() * 10.0;
    s.guardWidth = m_guardWidth->value() * 10.0;
    s.gripLength = m_gripLength->value() * 10.0;
    s.tip = BladeTip(m_tip->currentIndex());
    s.pommel = m_pommel->isChecked();
    s.shield = ShieldShape(m_shieldShape->currentIndex());
    s.shieldWidth = m_shieldWidth->value() * 10.0;
    s.shieldHeight = m_shieldHeight->value() * 10.0;
    s.shieldThickness = m_shieldThickness->value() * 10.0;
    s.shieldCurve = m_shieldCurve->value() / 100.0;
    s.boss = m_boss->isChecked();
    s.gripBar = m_gripBar->isChecked();
    s.staffLength = m_staffLength->value() * 10.0;
    s.staffEnd = StaffEnd(m_staffEnd->currentIndex());
    s.waistWidth = m_waistWidth->value();
    s.waistDepth = m_waistDepth->value();
    s.beltWidth = m_beltWidth->value() * 10.0;
    s.fit = m_fit->value();
    s.beltOpen = m_open->isChecked();
    s.beltGap = m_gap->value();
    s.holsterLength = m_holsterLength->value() * 10.0;
    s.holsterWidth = m_holsterWidth->value() * 10.0;
    s.holsterDepth = m_holsterDepth->value() * 10.0;
    return s;
}

void AccessoryPanel::remember() const
{
    const AccessorySettings s = settings();
    QSettings p = prefs();
    p.setValue(QStringLiteral("accessory/kind"), m_kind->currentIndex());
    p.setValue(QStringLiteral("accessory/scale"), m_scale->currentIndex());
    p.setValue(QStringLiteral("ports/handle"), s.handle);  // the same handle as the hands'
    const std::pair<const char*, double> values[] = {
        {"bladeLength", s.bladeLength},   {"bladeWidth", s.bladeWidth},       {"bladeThickness", s.bladeThickness},
        {"guardWidth", s.guardWidth},     {"gripLength", s.gripLength},       {"tip", double(int(s.tip))},
        {"pommel", s.pommel ? 1.0 : 0.0}, {"shield", double(int(s.shield))}, {"shieldWidth", s.shieldWidth},
        {"shieldHeight", s.shieldHeight}, {"shieldThickness", s.shieldThickness},
        {"shieldCurve", s.shieldCurve},   {"boss", s.boss ? 1.0 : 0.0},      {"gripBar", s.gripBar ? 1.0 : 0.0},
        {"staffLength", s.staffLength},   {"staffEnd", double(int(s.staffEnd))},
        {"waistWidth", s.waistWidth},     {"waistDepth", s.waistDepth},      {"beltWidth", s.beltWidth},
        {"fit", s.fit},                   {"beltOpen", s.beltOpen ? 1.0 : 0.0}, {"beltGap", s.beltGap},
        {"holsterLength", s.holsterLength}, {"holsterWidth", s.holsterWidth}, {"holsterDepth", s.holsterDepth}};
    for (const auto& [key, value] : values)
        p.setValue(QStringLiteral("accessory/") + QLatin1String(key), value);
}

void AccessoryPanel::recall()
{
    m_loading = true;
    const QSettings p = prefs();
    const AccessorySettings d;
    const auto get = [&p](const char* key, double fallback) {
        return p.value(QStringLiteral("accessory/") + QLatin1String(key), fallback).toDouble();
    };
    m_kind->setCurrentIndex(std::clamp(p.value(QStringLiteral("accessory/kind"), 0).toInt(), 0, kAccessoryKindCount - 1));
    m_scale->setCurrentIndex(std::clamp(p.value(QStringLiteral("accessory/scale"), 2).toInt(), 0, 3));
    m_handle->setValue(p.value(QStringLiteral("ports/handle"), d.handle).toDouble());
    m_bladeLength->setValue(get("bladeLength", d.bladeLength) / 10.0);
    m_bladeWidth->setValue(get("bladeWidth", d.bladeWidth) / 10.0);
    m_bladeThickness->setValue(get("bladeThickness", d.bladeThickness) / 10.0);
    m_guardWidth->setValue(get("guardWidth", d.guardWidth) / 10.0);
    m_gripLength->setValue(get("gripLength", d.gripLength) / 10.0);
    m_tip->setCurrentIndex(std::clamp(int(get("tip", 0)), 0, 2));
    m_pommel->setChecked(get("pommel", 1.0) != 0.0);
    m_shieldShape->setCurrentIndex(std::clamp(int(get("shield", int(d.shield))), 0, 2));
    m_shieldWidth->setValue(get("shieldWidth", d.shieldWidth) / 10.0);
    m_shieldHeight->setValue(get("shieldHeight", d.shieldHeight) / 10.0);
    m_shieldThickness->setValue(get("shieldThickness", d.shieldThickness) / 10.0);
    m_shieldCurve->setValue(get("shieldCurve", d.shieldCurve) * 100.0);
    m_boss->setChecked(get("boss", 1.0) != 0.0);
    m_gripBar->setChecked(get("gripBar", 1.0) != 0.0);
    m_staffLength->setValue(get("staffLength", d.staffLength) / 10.0);
    m_staffEnd->setCurrentIndex(std::clamp(int(get("staffEnd", 0)), 0, 2));
    m_waistWidth->setValue(get("waistWidth", d.waistWidth));
    m_waistDepth->setValue(get("waistDepth", d.waistDepth));
    m_beltWidth->setValue(get("beltWidth", d.beltWidth) / 10.0);
    m_fit->setValue(get("fit", d.fit));
    m_open->setChecked(get("beltOpen", 1.0) != 0.0);
    m_gap->setValue(get("beltGap", d.beltGap));
    m_gap->setEnabled(m_open->isChecked());
    m_holsterLength->setValue(get("holsterLength", d.holsterLength) / 10.0);
    m_holsterWidth->setValue(get("holsterWidth", d.holsterWidth) / 10.0);
    m_holsterDepth->setValue(get("holsterDepth", d.holsterDepth) / 10.0);
    m_shieldHeight->setEnabled(m_shieldShape->currentIndex() != 0);
    showKind();
    m_loading = false;
}

void AccessoryPanel::setLibrary(const QStringList& names)
{
    const int was = m_library->currentRow();
    m_library->clear();
    m_library->addItems(names);
    if (names.isEmpty()) {
        auto* none = new QListWidgetItem(tr("None saved yet"));
        none->setFlags(Qt::NoItemFlags);
        m_library->addItem(none);
    } else {
        m_library->setCurrentRow(std::clamp(was, 0, int(names.size()) - 1));
    }
    m_addOwn->setEnabled(!names.isEmpty());
}

int AccessoryPanel::libraryIndex() const
{
    return m_library->currentRow();
}

void AccessoryPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #c0392b;") : QString());
}

void AccessoryPanel::setApplyEnabled(bool on)
{
    m_apply->setEnabled(on);
}

}  // namespace forge::app
