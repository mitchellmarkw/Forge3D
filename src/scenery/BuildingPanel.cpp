// SPDX-License-Identifier: GPL-3.0-or-later
#include "BuildingPanel.h"

#include "forge/core/Building.h"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

namespace forge::scenery {
namespace {

QSettings preferences()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"),
                     QStringLiteral("Forge3D Scenery"));
}

QLabel* wrapped(const QString& text)
{
    auto* l = new QLabel(text);
    l->setWordWrap(true);
    return l;
}

QDoubleSpinBox* spin(double lo, double hi, int decimals, double step, const QString& suffix, const QString& tip)
{
    auto* b = new QDoubleSpinBox;
    b->setRange(lo, hi);
    b->setDecimals(decimals);
    b->setSingleStep(step);
    b->setSuffix(suffix);
    b->setKeyboardTracking(false);
    b->setToolTip(tip);
    return b;
}

// A list with its label resting on it.
QWidget* labelled(const QString& text, QWidget* field)
{
    auto* w = new QWidget;
    auto* column = new QVBoxLayout(w);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(new QLabel(text));
    column->addWidget(field);
    return w;
}

}  // namespace

BuildingPanel::BuildingPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    layout->addWidget(m_title);
    layout->addWidget(wrapped(tr("Sizes are real-world (millimetres); the building is made at the model scale.")));

    const QString real = tr(" mm");
    // The walls.
    auto* wallsBox = new QGroupBox(tr("Walls"));
    auto* walls = new QVBoxLayout(wallsBox);
    m_shape = new QComboBox;
    for (BuildingShape s : {BuildingShape::Rectangle, BuildingShape::L, BuildingShape::T, BuildingShape::U,
                            BuildingShape::Courtyard})
        m_shape->addItem(tr(building::title(s)));
    m_shape->setToolTip(tr("The building's footprint. Drag the orange posts at its corners in the view to size it "
                           "and its wings"));
    walls->addWidget(labelled(tr("Shape:"), m_shape));
    m_material = new QComboBox;
    for (WallMaterial m : {WallMaterial::Brick, WallMaterial::Stone, WallMaterial::Logs, WallMaterial::Planks,
                           WallMaterial::RoughStone, WallMaterial::Fieldstone})
        m_material->addItem(tr(building::title(m)));
    m_material->setToolTip(tr("What the walls are built of. Choosing one sets its usual sizes (a standard brick is "
                              "194 x 92 x 57 mm)"));
    walls->addWidget(labelled(tr("Material:"), m_material));
    auto* wallForm = new QFormLayout;
    m_width = spin(500, 200000, 0, 100, real, tr("The building's width outside, left to right (front wall)"));
    wallForm->addRow(tr("Width:"), m_width);
    m_depth = spin(500, 200000, 0, 100, real, tr("The building's depth outside, front to back"));
    wallForm->addRow(tr("Depth:"), m_depth);
    m_height = spin(300, 100000, 0, 100, real, tr("The walls' height, to where the roof sits"));
    wallForm->addRow(tr("Wall height:"), m_height);
    m_thickness = spin(20, 3000, 0, 10, real, tr("How thick the walls are"));
    m_thicknessLabel = new QLabel(tr("Wall thickness:"));
    wallForm->addRow(m_thicknessLabel, m_thickness);
    walls->addLayout(wallForm);
    m_wingFields = new QWidget;
    auto* wingForm = new QFormLayout(m_wingFields);
    wingForm->setContentsMargins(0, 0, 0, 0);
    m_wingDepth = spin(100, 200000, 0, 100, real, {});
    m_wingDepthLabel = new QLabel;
    wingForm->addRow(m_wingDepthLabel, m_wingDepth);
    m_wingWidth = spin(100, 200000, 0, 100, real, {});
    m_wingWidthLabel = new QLabel;
    wingForm->addRow(m_wingWidthLabel, m_wingWidth);
    wingForm->addRow(wrapped(tr("Or drag the orange posts at the corners in the view.")));
    walls->addWidget(m_wingFields);
    layout->addWidget(wallsBox);

    // Their units.
    m_unitsBox = new QGroupBox(tr("Bricks"));
    auto* unitForm = new QFormLayout(m_unitsBox);
    m_unitLength = spin(20, 5000, 0, 5, real, {});
    m_unitLengthLabel = new QLabel;
    unitForm->addRow(m_unitLengthLabel, m_unitLength);
    m_unitHeight = spin(10, 2000, 0, 5, real, {});
    m_unitHeightLabel = new QLabel;
    unitForm->addRow(m_unitHeightLabel, m_unitHeight);
    m_mortar = new QCheckBox(tr("Mortar between them (set back from the face)"));
    m_mortar->setToolTip(tr("With mortar: the units are spaced by the joint, the mortar set back. Without: laid tight, "
                            "the joints shown as fine lines"));
    unitForm->addRow(m_mortar);
    m_joint = spin(1, 100, 0, 1, real, tr("The mortar joint's width (widened if it's too fine to print)"));
    m_jointLabel = new QLabel(tr("Joint:"));
    unitForm->addRow(m_jointLabel, m_joint);
    layout->addWidget(m_unitsBox);

    // The roof.
    auto* roofBox = new QGroupBox(tr("Roof"));
    auto* roofLayout = new QVBoxLayout(roofBox);
    m_roof = new QComboBox;
    for (RoofKind r : {RoofKind::None, RoofKind::Flat, RoofKind::Gable, RoofKind::Hip})
        m_roof->addItem(tr(building::title(r)));
    m_roof->setToolTip(tr("Gable: two slopes, the ridge along the longer side. Hip: four slopes"));
    roofLayout->addWidget(labelled(tr("Roof:"), m_roof));
    m_roofDetails = new QWidget;
    auto* roofForm = new QFormLayout(m_roofDetails);
    roofForm->setContentsMargins(0, 0, 0, 0);
    m_pitch = spin(5, 75, 0, 5, tr("°"), tr("How steep the slopes are"));
    roofForm->addRow(tr("Pitch:"), m_pitch);
    m_overhang = spin(0, 3000, 0, 50, real, tr("How far the roof reaches past the walls, all round"));
    roofForm->addRow(tr("Overhang:"), m_overhang);
    m_roofSeparate = new QCheckBox(tr("The roof its own shape (it lifts off)"));
    m_roofSeparate->setToolTip(tr("Printed apart and set on; lift it off to see or furnish inside"));
    roofForm->addRow(m_roofSeparate);
    roofLayout->addWidget(m_roofDetails);
    layout->addWidget(roofBox);

    // Battlements and towers (slice 117).
    auto* castleBox = new QGroupBox(tr("Battlements and towers"));
    auto* castle = new QVBoxLayout(castleBox);
    m_battlements = new QCheckBox(tr("Battlements along the wall tops"));
    m_battlements->setToolTip(tr("The walls carried up past the roof as a notched parapet; the roof sits inside it"));
    castle->addWidget(m_battlements);
    m_parapetFields = new QWidget;
    auto* parapetForm = new QFormLayout(m_parapetFields);
    parapetForm->setContentsMargins(0, 0, 0, 0);
    m_parapet = spin(300, 5000, 0, 50, real, tr("How high the parapet stands above the roof (the walkway)"));
    parapetForm->addRow(tr("Parapet height:"), m_parapet);
    m_merlon = spin(100, 10000, 0, 50, real, tr("The raised parts' width"));
    parapetForm->addRow(tr("Merlon width:"), m_merlon);
    m_crenel = spin(100, 10000, 0, 50, real, tr("The gaps' width (stretched a little to fit each wall)"));
    parapetForm->addRow(tr("Crenel width:"), m_crenel);
    castle->addWidget(m_parapetFields);
    m_addTower = new QPushButton(tr("Add Tower"));
    m_addTower->setCheckable(true);
    m_addTower->setToolTip(tr("Then click a corner's post or a wall: the tower goes there"));
    castle->addWidget(m_addTower);
    castle->addWidget(wrapped(tr("Click a corner's post or a wall to put it there (or near: it snaps). Drag a tower "
                                 "to move it.")));
    m_towers = new QListWidget;
    m_towers->setMaximumHeight(90);
    castle->addWidget(m_towers);
    m_towerDetails = new QWidget;
    auto* towerLayout = new QVBoxLayout(m_towerDetails);
    towerLayout->setContentsMargins(0, 0, 0, 0);
    m_towerShape = new QComboBox;
    for (TowerShape t : {TowerShape::Round, TowerShape::Square})
        m_towerShape->addItem(tr(building::title(t)));
    towerLayout->addWidget(labelled(tr("Tower shape:"), m_towerShape));
    auto* towerForm = new QFormLayout;
    m_towerSize = spin(100, 50000, 0, 100, real, tr("Across the tower"));
    towerForm->addRow(tr("Across:"), m_towerSize);
    m_towerHeight = spin(1000, 200000, 0, 100, real, tr("To its top (its walkway, under battlements)"));
    towerForm->addRow(tr("Height:"), m_towerHeight);
    towerLayout->addLayout(towerForm);
    m_towerTop = new QComboBox;
    for (TowerTop t : {TowerTop::Flat, TowerTop::Battlements, TowerTop::Roof})
        m_towerTop->addItem(tr(building::title(t)));
    m_towerTop->setToolTip(tr("A round tower's roof is a cone, a square one's a pyramid"));
    towerLayout->addWidget(labelled(tr("Top:"), m_towerTop));
    auto* removeTower = new QPushButton(tr("Remove Tower"));
    towerLayout->addWidget(removeTower);
    castle->addWidget(m_towerDetails);
    layout->addWidget(castleBox);

    // Doors and windows.
    auto* openBox = new QGroupBox(tr("Doors and windows"));
    auto* open = new QVBoxLayout(openBox);
    auto* buttons = new QHBoxLayout;
    m_addDoor = new QPushButton(tr("Add Door"));
    m_addDoor->setCheckable(true);
    m_addDoor->setToolTip(tr("Then click on a wall where the door goes"));
    m_addWindow = new QPushButton(tr("Add Window"));
    m_addWindow->setCheckable(true);
    m_addWindow->setToolTip(tr("Then click on a wall where the window goes"));
    buttons->addWidget(m_addDoor);
    buttons->addWidget(m_addWindow);
    open->addLayout(buttons);
    m_placeHint = wrapped(tr("Click on a wall to put it there. Click one to pick it; drag it to move it."));
    open->addWidget(m_placeHint);
    m_openings = new QListWidget;
    m_openings->setMaximumHeight(110);
    open->addWidget(m_openings);
    m_openingDetails = new QWidget;
    auto* openForm = new QFormLayout(m_openingDetails);
    openForm->setContentsMargins(0, 0, 0, 0);
    m_openWidth = spin(100, 20000, 0, 50, real, tr("The opening's width"));
    openForm->addRow(tr("Width:"), m_openWidth);
    m_openHeight = spin(100, 20000, 0, 50, real, tr("The opening's height"));
    openForm->addRow(tr("Height:"), m_openHeight);
    m_sill = spin(0, 20000, 0, 50, real, tr("How high the window's bottom is off the floor"));
    m_sillLabel = new QLabel(tr("Sill height:"));
    openForm->addRow(m_sillLabel, m_sill);
    auto* remove = new QPushButton(tr("Remove"));
    remove->setToolTip(tr("Take the picked door or window out (Delete)"));
    openForm->addRow(remove);
    open->addWidget(m_openingDetails);
    layout->addWidget(openBox);

    auto* printForm = new QFormLayout;
    m_smallest = spin(0.05, 2.0, 2, 0.05, tr(" mm"),
                      tr("The finest detail your printer makes (a nozzle's width, or about 0.1 mm on a resin "
                         "printer): mortar lines finer than this are widened, units too small are left plain"));
    printForm->addRow(tr("Smallest detail:"), m_smallest);
    layout->addLayout(printForm);

    m_size = wrapped({});
    layout->addWidget(m_size);
    m_notes = wrapped({});
    m_notes->setStyleSheet(QStringLiteral("color: #a06000;"));
    layout->addWidget(m_notes);
    m_status = wrapped({});
    m_status->hide();
    layout->addWidget(m_status);
    auto* actions = new QHBoxLayout;
    auto* cancel = new QPushButton(tr("Cancel"));
    m_apply = new QPushButton;
    m_apply->setDefault(true);
    actions->addStretch(1);
    actions->addWidget(cancel);
    actions->addWidget(m_apply);
    layout->addLayout(actions);
    layout->addStretch(1);

    // Any field: the description again.
    for (QDoubleSpinBox* b : {m_width, m_depth, m_height, m_thickness, m_unitLength, m_unitHeight, m_joint, m_pitch,
                              m_overhang, m_smallest, m_wingDepth, m_wingWidth, m_parapet, m_merlon, m_crenel})
        connect(b, &QDoubleSpinBox::valueChanged, this, [this] {
            if (m_loading)
                return;
            readFields();
            emit changed();
        });
    for (QCheckBox* c : {m_mortar, m_roofSeparate})
        connect(c, &QCheckBox::toggled, this, [this] {
            if (m_loading)
                return;
            readFields();
            showMaterial();
            emit changed();
        });
    connect(m_roof, &QComboBox::currentIndexChanged, this, [this] {
        if (m_loading)
            return;
        readFields();
        showMaterial();
        emit changed();
    });
    connect(m_shape, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (m_loading)
            return;
        readFields();
        m_spec = building::withShape(m_spec, BuildingShape(i));  // its usual wings; doors and windows go
        m_selected = -1;
        load();
        emit changed();
        emit selectionChanged(-1);
    });
    connect(m_material, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (m_loading)
            return;
        readFields();
        m_spec = building::defaults(WallMaterial(i), m_spec);  // its usual sizes
        load();
        emit changed();
    });
    connect(m_battlements, &QCheckBox::toggled, this, [this] {
        if (m_loading)
            return;
        readFields();
        showParapet();
        emit changed();
    });
    connect(m_addTower, &QPushButton::clicked, this, [this](bool on) {
        setPlacing(on ? kPlaceTower : -1);
        emit placeRequested(on ? kPlaceTower : -1);
    });
    connect(m_towers, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_loading)
            return;
        m_tower = row;
        showTower();
        emit towerSelectionChanged(row);
    });
    const auto towerEdited = [this] {
        if (m_loading || m_tower < 0 || m_tower >= int(m_spec.towers.size()))
            return;
        BuildingTower& t = m_spec.towers[std::size_t(m_tower)];
        t.shape = TowerShape(m_towerShape->currentIndex());
        t.size = std::max(m_towerSize->value(), 2.5 * (m_spec.material == WallMaterial::Logs ? m_spec.unitHeight : m_spec.wallThickness));
        t.height = m_towerHeight->value();
        t.top = TowerTop(m_towerTop->currentIndex());
        fillTowers();
        showParapet();
        emit changed();
    };
    for (QDoubleSpinBox* b : {m_towerSize, m_towerHeight})
        connect(b, &QDoubleSpinBox::valueChanged, this, towerEdited);
    for (QComboBox* c : {m_towerShape, m_towerTop})
        connect(c, &QComboBox::currentIndexChanged, this, towerEdited);
    connect(removeTower, &QPushButton::clicked, this, [this] {
        if (m_tower < 0 || m_tower >= int(m_spec.towers.size()))
            return;
        m_spec.towers.erase(m_spec.towers.begin() + m_tower);
        m_tower = std::min(m_tower, int(m_spec.towers.size()) - 1);
        fillTowers();
        showParapet();
        emit changed();
        emit towerSelectionChanged(m_tower);
    });
    connect(m_addDoor, &QPushButton::clicked, this, [this](bool on) {
        setPlacing(on ? int(OpeningKind::Door) : -1);
        emit placeRequested(on ? int(OpeningKind::Door) : -1);
    });
    connect(m_addWindow, &QPushButton::clicked, this, [this](bool on) {
        setPlacing(on ? int(OpeningKind::Window) : -1);
        emit placeRequested(on ? int(OpeningKind::Window) : -1);
    });
    connect(m_openings, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_loading)
            return;
        m_selected = row;
        showOpening();
        emit selectionChanged(row);
    });
    for (QDoubleSpinBox* b : {m_openWidth, m_openHeight, m_sill})
        connect(b, &QDoubleSpinBox::valueChanged, this, [this] {
            if (m_loading || m_selected < 0 || m_selected >= int(m_spec.openings.size()))
                return;
            BuildingOpening& o = m_spec.openings[std::size_t(m_selected)];
            o.width = m_openWidth->value();
            o.height = m_openHeight->value();
            if (o.kind == OpeningKind::Window)
                o.bottom = m_sill->value();
            o = building::keptIn(m_spec, o);
            fillOpenings();
            emit changed();
        });
    connect(remove, &QPushButton::clicked, this, [this] {
        if (m_selected < 0 || m_selected >= int(m_spec.openings.size()))
            return;
        m_spec.openings.erase(m_spec.openings.begin() + m_selected);
        m_selected = std::min(m_selected, int(m_spec.openings.size()) - 1);
        fillOpenings();
        emit changed();
        emit selectionChanged(m_selected);
    });
    for (QWidget* w : findChildren<QAbstractSpinBox*>()) {
        w->setFocusPolicy(Qt::StrongFocus);
        w->installEventFilter(this);
    }
    for (QWidget* w : findChildren<QComboBox*>()) {
        w->setFocusPolicy(Qt::StrongFocus);
        w->installEventFilter(this);
    }
    connect(m_apply, &QPushButton::clicked, this, &BuildingPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &BuildingPanel::cancelRequested);
}

bool BuildingPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Wheel) {
        if (auto* w = qobject_cast<QWidget*>(watched); w && !w->hasFocus()) {
            event->ignore();
            return true;  // to the scroll area
        }
    }
    return QWidget::eventFilter(watched, event);
}

void BuildingPanel::start(const BuildingSpec& spec, bool editing)
{
    m_spec = spec;
    if (!editing)
        m_spec.smallest = preferences().value(QStringLiteral("building/smallest"), 0.4).toDouble();
    m_selected = -1;
    m_tower = -1;
    m_title->setText(editing ? tr("Change the building") : tr("New building"));
    m_apply->setText(editing ? tr("Update Building") : tr("Add Building"));
    setPlacing(-1);
    load();
    setStatus({}, false);
}

void BuildingPanel::setSpec(const BuildingSpec& spec)
{
    m_spec = spec;
    load();
}

void BuildingPanel::selectOpening(int index)
{
    m_selected = index;
    const QSignalBlocker block(m_openings);
    m_openings->setCurrentRow(index);
    showOpening();
}

void BuildingPanel::setPlacing(int kind)
{
    const QSignalBlocker a(m_addDoor), b(m_addWindow), c(m_addTower);
    m_addDoor->setChecked(kind == int(OpeningKind::Door));
    m_addWindow->setChecked(kind == int(OpeningKind::Window));
    m_addTower->setChecked(kind == kPlaceTower);
}

void BuildingPanel::selectTower(int index)
{
    m_tower = index;
    const QSignalBlocker block(m_towers);
    m_towers->setCurrentRow(index);
    showTower();
}

void BuildingPanel::fillTowers()
{
    const bool was = m_loading;
    m_loading = true;
    m_towers->clear();
    for (const auto& t : m_spec.towers)
        m_towers->addItem(tr("%1 tower, %2 mm across, %3 mm high, %4")
                              .arg(tr(building::title(t.shape)))
                              .arg(t.size, 0, 'f', 0)
                              .arg(t.height, 0, 'f', 0)
                              .arg(tr(building::title(t.top)).toLower()));
    if (m_tower >= int(m_spec.towers.size()))
        m_tower = -1;
    m_towers->setCurrentRow(m_tower);
    m_loading = was;
    showTower();
}

void BuildingPanel::showTower()
{
    const bool any = m_tower >= 0 && m_tower < int(m_spec.towers.size());
    m_towerDetails->setVisible(any);
    if (!any)
        return;
    const BuildingTower& t = m_spec.towers[std::size_t(m_tower)];
    const bool was = m_loading;
    m_loading = true;
    m_towerShape->setCurrentIndex(int(t.shape));
    m_towerSize->setValue(t.size);
    m_towerHeight->setValue(t.height);
    m_towerTop->setCurrentIndex(int(t.top));
    m_loading = was;
}

// The parapet's sizes: shown when the walls or a tower have battlements.
void BuildingPanel::showParapet()
{
    bool any = m_spec.battlements;
    for (const auto& t : m_spec.towers)
        any = any || t.top == TowerTop::Battlements;
    m_parapetFields->setVisible(any);
}

void BuildingPanel::setNotes(const QStringList& notes)
{
    m_notes->setText(notes.join(QLatin1Char('\n')));
    m_notes->setVisible(!notes.isEmpty());
}

void BuildingPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #d04020;") : QString());
    m_status->setVisible(!text.isEmpty());
}

void BuildingPanel::load()
{
    m_loading = true;
    m_material->setCurrentIndex(int(m_spec.material));
    m_shape->setCurrentIndex(int(m_spec.shape));
    m_wingDepth->setValue(m_spec.wingDepth);
    m_wingWidth->setValue(m_spec.wingWidth);
    m_width->setValue(m_spec.width);
    m_depth->setValue(m_spec.depth);
    m_height->setValue(m_spec.height);
    m_thickness->setValue(m_spec.wallThickness);
    m_unitLength->setValue(std::max(20.0, m_spec.unitLength));
    m_unitHeight->setValue(m_spec.unitHeight);
    m_mortar->setChecked(m_spec.mortar);
    m_joint->setValue(std::max(1.0, m_spec.joint));
    m_roof->setCurrentIndex(int(m_spec.roof));
    m_pitch->setValue(m_spec.pitch);
    m_overhang->setValue(m_spec.overhang);
    m_roofSeparate->setChecked(m_spec.roofSeparate);
    m_smallest->setValue(m_spec.smallest);
    m_battlements->setChecked(m_spec.battlements);
    m_parapet->setValue(m_spec.parapetHeight);
    m_merlon->setValue(m_spec.merlonWidth);
    m_crenel->setValue(m_spec.crenelWidth);
    m_loading = false;
    showMaterial();
    fillOpenings();
    fillTowers();
    showParapet();
}

void BuildingPanel::readFields()
{
    m_spec.material = WallMaterial(m_material->currentIndex());
    m_spec.shape = BuildingShape(m_shape->currentIndex());
    m_spec.wingDepth = m_wingDepth->value();
    m_spec.wingWidth = m_wingWidth->value();
    m_spec.width = m_width->value();
    m_spec.depth = m_depth->value();
    m_spec.height = m_height->value();
    m_spec.wallThickness = m_thickness->value();
    const WallMaterial mat = m_spec.material;
    if (mat == WallMaterial::Brick || mat == WallMaterial::Stone || mat == WallMaterial::RoughStone ||
        mat == WallMaterial::Fieldstone)
        m_spec.unitLength = m_unitLength->value();
    m_spec.unitHeight = mat == WallMaterial::Fieldstone ? m_spec.unitLength : m_unitHeight->value();
    m_spec.mortar = m_mortar->isChecked();
    m_spec.joint = m_joint->value();
    m_spec.roof = RoofKind(m_roof->currentIndex());
    m_spec.pitch = m_pitch->value();
    m_spec.overhang = m_overhang->value();
    m_spec.roofSeparate = m_roofSeparate->isChecked();
    m_spec.smallest = m_smallest->value();
    m_spec.battlements = m_battlements->isChecked();
    m_spec.parapetHeight = m_parapet->value();
    m_spec.merlonWidth = m_merlon->value();
    m_spec.crenelWidth = m_crenel->value();
    preferences().setValue(QStringLiteral("building/smallest"), m_spec.smallest);
    // The wings kept buildable (a smaller building, thicker walls): shown as they are.
    m_spec = building::kept(m_spec);
    {
        const QSignalBlocker a(m_wingDepth), b(m_wingWidth);
        m_wingDepth->setValue(m_spec.wingDepth);
        m_wingWidth->setValue(m_spec.wingWidth);
    }
    for (auto& o : m_spec.openings)
        o = building::keptIn(m_spec, o);
}

void BuildingPanel::showMaterial()
{
    const WallMaterial m = m_spec.material;
    const bool field = m == WallMaterial::Fieldstone;
    const bool laid = m == WallMaterial::Brick || m == WallMaterial::Stone || m == WallMaterial::RoughStone || field;
    m_unitsBox->setTitle(m == WallMaterial::Brick        ? tr("Bricks")
                         : m == WallMaterial::Stone      ? tr("Stone blocks")
                         : m == WallMaterial::RoughStone ? tr("Rough-hewn stones")
                         : field                         ? tr("Fieldstones")
                         : m == WallMaterial::Logs  ? tr("Logs")
                                                    : tr("Planks"));
    m_unitLengthLabel->setText(m == WallMaterial::Stone        ? tr("Block length (average):")
                               : m == WallMaterial::RoughStone ? tr("Stone length (average):")
                               : field                         ? tr("Stone size (average):")
                                                               : tr("Brick length:"));
    m_unitHeightLabel->setText(m == WallMaterial::Brick   ? tr("Brick height:")
                               : m == WallMaterial::Stone || m == WallMaterial::RoughStone ? tr("Course height (average):")
                               : m == WallMaterial::Logs  ? tr("Log diameter:")
                                                          : tr("Plank width (showing):"));
    m_unitHeight->setToolTip(m == WallMaterial::Logs ? tr("The logs' thickness: the walls are this thick")
                                                     : tr("How tall each course is"));
    m_unitHeightLabel->setVisible(!field);
    m_unitHeight->setVisible(!field);
    m_unitLengthLabel->setVisible(laid);
    m_unitLength->setVisible(laid);
    m_mortar->setVisible(laid);
    m_jointLabel->setVisible(laid && m_spec.mortar);
    m_joint->setVisible(laid && m_spec.mortar);
    m_thicknessLabel->setVisible(m != WallMaterial::Logs);
    m_thickness->setVisible(m != WallMaterial::Logs);
    m_roofDetails->setVisible(m_spec.roof != RoofKind::None);
    m_wingFields->setVisible(m_spec.shape != BuildingShape::Rectangle);
    switch (m_spec.shape) {
    case BuildingShape::Rectangle: break;
    case BuildingShape::L:
        m_wingDepthLabel->setText(tr("Front wing depth:"));
        m_wingWidthLabel->setText(tr("Side wing width:"));
        break;
    case BuildingShape::T:
        m_wingDepthLabel->setText(tr("Back bar depth:"));
        m_wingWidthLabel->setText(tr("Stem width:"));
        break;
    case BuildingShape::U:
        m_wingDepthLabel->setText(tr("Back bar depth:"));
        m_wingWidthLabel->setText(tr("Arm width:"));
        break;
    case BuildingShape::Courtyard:
        m_wingDepthLabel->setText(tr("Front and back wings:"));
        m_wingWidthLabel->setText(tr("Side wings:"));
        break;
    }
    m_wingDepth->setToolTip(tr("How deep the wing is, front to back"));
    m_wingWidth->setToolTip(tr("How wide the wing is, side to side"));
    const double k = 1.0 / m_spec.scale;
    m_size->setText(tr("On the plate: %1 x %2 x %3 mm walls, at 1:%4")
                        .arg(m_spec.width * k, 0, 'f', 1)
                        .arg(m_spec.depth * k, 0, 'f', 1)
                        .arg(m_spec.height * k, 0, 'f', 1)
                        .arg(m_spec.scale));
}

void BuildingPanel::fillOpenings()
{
    const bool was = m_loading;
    m_loading = true;
    m_openings->clear();
    for (const auto& o : m_spec.openings)
        m_openings->addItem(tr("%1, %2 wall, %3 x %4 mm")
                                .arg(o.kind == OpeningKind::Door ? tr("Door") : tr("Window"),
                                     QString::fromStdString(building::wallName(m_spec, o.wall)))
                                .arg(o.width, 0, 'f', 0)
                                .arg(o.height, 0, 'f', 0));
    if (m_selected >= int(m_spec.openings.size()))
        m_selected = -1;
    m_openings->setCurrentRow(m_selected);
    m_loading = was;
    showOpening();
}

void BuildingPanel::showOpening()
{
    const bool any = m_selected >= 0 && m_selected < int(m_spec.openings.size());
    m_openingDetails->setVisible(any);
    if (!any)
        return;
    const BuildingOpening& o = m_spec.openings[std::size_t(m_selected)];
    const bool was = m_loading;
    m_loading = true;
    m_openWidth->setValue(o.width);
    m_openHeight->setValue(o.height);
    m_sill->setValue(o.bottom);
    m_sillLabel->setVisible(o.kind == OpeningKind::Window);
    m_sill->setVisible(o.kind == OpeningKind::Window);
    m_loading = was;
}

}  // namespace forge::scenery
