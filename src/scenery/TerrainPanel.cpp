// SPDX-License-Identifier: GPL-3.0-or-later
#include "TerrainPanel.h"

#include "forge/core/Terrain.h"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

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

QSpinBox* count(int hi, const QString& tip)
{
    auto* b = new QSpinBox;
    b->setRange(0, hi);
    b->setKeyboardTracking(false);
    b->setToolTip(tip);
    return b;
}

// A list with its label resting on it.
QVBoxLayout* labelled(const QString& label, QWidget* field)
{
    auto* column = new QVBoxLayout;
    column->setSpacing(0);
    column->addWidget(new QLabel(label));
    column->addWidget(field);
    return column;
}

}  // namespace

TerrainPanel::TerrainPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    layout->addWidget(m_title);
    layout->addWidget(wrapped(tr("Ground sizes are real-world (millimetres), made at the model scale; the base's are "
                                 "the model's own.")));
    const QString real = tr(" mm");

    // The base.
    auto* baseBox = new QGroupBox(tr("Base"));
    auto* base = new QVBoxLayout(baseBox);
    m_base = new QComboBox;
    m_base->addItem(tr("Rectangle"));
    m_base->addItem(tr("Round or oval"));
    m_base->setToolTip(tr("A rectangle, or a round or oval base (a circle when width and depth are the same)"));
    base->addLayout(labelled(tr("Shape:"), m_base));
    auto* baseForm = new QFormLayout;
    m_width = spin(100, 1000000, 0, 500, real, tr("Its width, left to right (real-world)"));
    baseForm->addRow(tr("Width:"), m_width);
    m_depth = spin(100, 1000000, 0, 500, real, tr("Its depth, front to back (real-world)"));
    baseForm->addRow(tr("Depth:"), m_depth);
    m_thickness = spin(0.5, 50, 1, 0.5, tr(" mm"), tr("How thick the flat slab under the ground is, on the model"));
    baseForm->addRow(tr("Base thickness:"), m_thickness);
    base->addLayout(baseForm);
    m_edge = new QComboBox;
    m_edge->addItem(tr("Plain (straight sides)"));
    m_edge->addItem(tr("Bevelled (a sloped plinth)"));
    m_edge->setToolTip(tr("Straight sides, or sides sloping in, like a display plinth"));
    base->addLayout(labelled(tr("Edge:"), m_edge));
    auto* bevelForm = new QFormLayout;
    m_bevel = spin(0.5, 30, 1, 0.5, tr(" mm"), tr("How far the bevelled edge slopes in, on the model"));
    m_bevelLabel = new QLabel(tr("Bevel:"));
    bevelForm->addRow(m_bevelLabel, m_bevel);
    base->addLayout(bevelForm);
    layout->addWidget(baseBox);

    // The ground.
    auto* groundBox = new QGroupBox(tr("Ground"));
    auto* ground = new QVBoxLayout(groundBox);
    auto* groundForm = new QFormLayout;
    m_hillHeight = spin(0, 100000, 0, 100, real, tr("How high the hills rise (0: flat ground)"));
    groundForm->addRow(tr("Hill height:"), m_hillHeight);
    m_hillSize = spin(100, 1000000, 0, 500, real, tr("About how far across a hill is"));
    groundForm->addRow(tr("Hill size:"), m_hillSize);
    auto* roughRow = new QHBoxLayout;
    m_rough = new QSlider(Qt::Horizontal);
    m_rough->setRange(0, 100);
    m_roughBox = new QSpinBox;
    m_roughBox->setRange(0, 100);
    m_roughBox->setSuffix(tr(" %"));
    m_roughBox->setKeyboardTracking(false);
    m_roughBox->setToolTip(tr("How bumpy the ground's surface is"));
    roughRow->addWidget(m_rough, 1);
    roughRow->addWidget(m_roughBox);
    groundForm->addRow(tr("Roughness:"), roughRow);
    ground->addLayout(groundForm);
    m_levelEdges = new QCheckBox(tr("Level at the edges (pieces line up)"));
    m_levelEdges->setToolTip(tr("The hills fade out at the edge, so terrain pieces put side by side meet at the "
                                "same height"));
    ground->addWidget(m_levelEdges);
    auto* seedRow = new QHBoxLayout;
    auto* another = new QPushButton(tr("New Hills"));
    another->setToolTip(tr("Other hills of the same height and size"));
    m_seed = new QSpinBox;
    m_seed->setRange(1, 999999);
    m_seed->setKeyboardTracking(false);
    m_seed->setToolTip(tr("Its number: the same number makes the same hills again"));
    seedRow->addWidget(another);
    seedRow->addWidget(new QLabel(tr("Number:")));
    seedRow->addWidget(m_seed);
    ground->addLayout(seedRow);
    layout->addWidget(groundBox);

    // Craters, crags and paths.
    auto* featBox = new QGroupBox(tr("Craters, crags and paths"));
    auto* feat = new QVBoxLayout(featBox);
    auto* buttons = new QHBoxLayout;
    m_addCrater = new QPushButton(tr("Add Crater"));
    m_addCrag = new QPushButton(tr("Add Crag"));
    m_addPath = new QPushButton(tr("Add Path"));
    m_addCrater->setToolTip(tr("Then click on the ground where the crater goes"));
    m_addCrag->setToolTip(tr("Then click on the ground where the rock outcrop goes"));
    m_addPath->setToolTip(tr("Then click where the path starts, and where it ends"));
    for (QPushButton* b : {m_addCrater, m_addCrag, m_addPath}) {
        b->setCheckable(true);
        buttons->addWidget(b);
    }
    feat->addLayout(buttons);
    feat->addWidget(wrapped(tr("Click one on the ground to pick it; drag it to move it (a path's end, near it).")));
    m_features = new QListWidget;
    m_features->setMaximumHeight(120);
    feat->addWidget(m_features);
    m_featureDetails = new QWidget;
    auto* featForm = new QFormLayout(m_featureDetails);
    featForm->setContentsMargins(0, 0, 0, 0);
    m_featureSize = spin(20, 100000, 0, 100, real, {});
    m_featureSizeLabel = new QLabel;
    featForm->addRow(m_featureSizeLabel, m_featureSize);
    m_featureDepth = spin(0, 100000, 0, 50, real, {});
    m_featureDepthLabel = new QLabel;
    featForm->addRow(m_featureDepthLabel, m_featureDepth);
    auto* remove = new QPushButton(tr("Remove"));
    remove->setToolTip(tr("Take the picked one out (Delete)"));
    featForm->addRow(remove);
    feat->addWidget(m_featureDetails);
    feat->addWidget(wrapped(tr("<b>Scatter</b> puts these at random (again for other places); the ones you placed "
                               "stay.")));
    auto* scatterForm = new QFormLayout;
    m_craters = count(40, tr("How many craters Scatter puts"));
    scatterForm->addRow(tr("Craters:"), m_craters);
    m_crags = count(40, tr("How many crags Scatter puts"));
    scatterForm->addRow(tr("Crags:"), m_crags);
    m_paths = count(6, tr("How many paths Scatter puts across"));
    scatterForm->addRow(tr("Paths:"), m_paths);
    feat->addLayout(scatterForm);
    auto* scatterRow = new QHBoxLayout;
    auto* scatter = new QPushButton(tr("Scatter"));
    scatter->setToolTip(tr("Put the craters, crags and paths at random"));
    auto* clear = new QPushButton(tr("Clear Scattered"));
    clear->setToolTip(tr("Take out the scattered ones (the ones you placed stay)"));
    scatterRow->addWidget(scatter);
    scatterRow->addWidget(clear);
    feat->addLayout(scatterRow);
    layout->addWidget(featBox);

    auto* printForm = new QFormLayout;
    m_smallest = spin(0.05, 2.0, 2, 0.05, tr(" mm"), tr("The finest detail your printer makes"));
    printForm->addRow(tr("Smallest detail:"), m_smallest);
    layout->addLayout(printForm);
    m_size = wrapped({});
    layout->addWidget(m_size);
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

    const auto changedHere = [this] {
        if (m_loading)
            return;
        readFields();
        showFields();
        emit changed();
    };
    for (QDoubleSpinBox* b : {m_width, m_depth, m_thickness, m_bevel, m_hillHeight, m_hillSize, m_smallest})
        connect(b, &QDoubleSpinBox::valueChanged, this, changedHere);
    for (QSpinBox* b : {m_roughBox, m_seed})
        connect(b, &QSpinBox::valueChanged, this, changedHere);
    for (QComboBox* c : {m_base, m_edge})
        connect(c, &QComboBox::currentIndexChanged, this, changedHere);
    connect(m_levelEdges, &QCheckBox::toggled, this, changedHere);
    connect(m_rough, &QSlider::valueChanged, this, [this](int v) {
        const QSignalBlocker block(m_roughBox);
        m_roughBox->setValue(v);
        if (!m_rough->isSliderDown() && !m_loading) {
            readFields();
            emit changed();
        }
    });
    connect(m_rough, &QSlider::sliderReleased, this, [this] {
        readFields();
        emit changed();
    });
    connect(m_roughBox, &QSpinBox::valueChanged, this, [this](int v) {
        const QSignalBlocker block(m_rough);
        m_rough->setValue(v);
    });
    connect(another, &QPushButton::clicked, this,
            [this] { m_seed->setValue(int(QRandomGenerator::global()->bounded(1, 999999))); });
    const auto placer = [this](QPushButton* b, TerrainFeatureKind k) {
        connect(b, &QPushButton::clicked, this, [this, k](bool on) {
            setPlacing(on ? int(k) : -1);
            emit placeRequested(on ? int(k) : -1);
        });
    };
    placer(m_addCrater, TerrainFeatureKind::Crater);
    placer(m_addCrag, TerrainFeatureKind::Crag);
    placer(m_addPath, TerrainFeatureKind::Path);
    connect(m_features, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_loading)
            return;
        m_selected = row;
        showFeature();
        emit selectionChanged(row);
    });
    for (QDoubleSpinBox* b : {m_featureSize, m_featureDepth})
        connect(b, &QDoubleSpinBox::valueChanged, this, [this] {
            if (m_loading || m_selected < 0 || m_selected >= int(m_spec.features.size()))
                return;
            TerrainFeature& f = m_spec.features[std::size_t(m_selected)];
            f.size = m_featureSize->value();
            f.depth = m_featureDepth->value();
            f.scattered = false;  // changed by hand: it stays
            fillFeatures();
            emit changed();
        });
    connect(remove, &QPushButton::clicked, this, [this] {
        if (m_selected < 0 || m_selected >= int(m_spec.features.size()))
            return;
        m_spec.features.erase(m_spec.features.begin() + m_selected);
        m_selected = std::min(m_selected, int(m_spec.features.size()) - 1);
        fillFeatures();
        emit changed();
        emit selectionChanged(m_selected);
    });
    const auto scatterNow = [this](bool none) {
        readFields();
        terrain::scatter(m_spec, none ? 0 : m_craters->value(), none ? 0 : m_crags->value(),
                         none ? 0 : m_paths->value(), QRandomGenerator::global()->generate());
        QSettings s = preferences();
        s.setValue(QStringLiteral("terrain/craters"), m_craters->value());
        s.setValue(QStringLiteral("terrain/crags"), m_crags->value());
        s.setValue(QStringLiteral("terrain/paths"), m_paths->value());
        m_selected = -1;
        fillFeatures();
        emit changed();
        emit selectionChanged(-1);
    };
    connect(scatter, &QPushButton::clicked, this, [scatterNow] { scatterNow(false); });
    connect(clear, &QPushButton::clicked, this, [scatterNow] { scatterNow(true); });
    for (QWidget* w : findChildren<QAbstractSpinBox*>()) {
        w->setFocusPolicy(Qt::StrongFocus);
        w->installEventFilter(this);
    }
    for (QWidget* w : findChildren<QComboBox*>()) {
        w->setFocusPolicy(Qt::StrongFocus);
        w->installEventFilter(this);
    }
    m_rough->setFocusPolicy(Qt::StrongFocus);
    m_rough->installEventFilter(this);
    connect(m_apply, &QPushButton::clicked, this, &TerrainPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &TerrainPanel::cancelRequested);
}

bool TerrainPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Wheel) {
        if (auto* w = qobject_cast<QWidget*>(watched); w && !w->hasFocus()) {
            event->ignore();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TerrainPanel::start(const TerrainSpec& spec, bool editing)
{
    m_spec = spec;
    QSettings s = preferences();
    if (!editing) {
        m_spec.smallest = s.value(QStringLiteral("building/smallest"), 0.4).toDouble();
        m_spec.seed = std::uint32_t(QRandomGenerator::global()->bounded(1, 999999));
    }
    {
        const QSignalBlocker a(m_craters), b(m_crags), c(m_paths);
        m_craters->setValue(s.value(QStringLiteral("terrain/craters"), 3).toInt());
        m_crags->setValue(s.value(QStringLiteral("terrain/crags"), 2).toInt());
        m_paths->setValue(s.value(QStringLiteral("terrain/paths"), 1).toInt());
    }
    m_selected = -1;
    m_title->setText(editing ? tr("Change the terrain") : tr("New terrain"));
    m_apply->setText(editing ? tr("Update Terrain") : tr("Add Terrain"));
    setPlacing(-1);
    load();
    setStatus({}, false);
}

void TerrainPanel::setSpec(const TerrainSpec& spec)
{
    m_spec = spec;
    load();
}

void TerrainPanel::selectFeature(int index)
{
    m_selected = index;
    const QSignalBlocker block(m_features);
    m_features->setCurrentRow(index);
    showFeature();
}

void TerrainPanel::setPlacing(int kind)
{
    const QSignalBlocker a(m_addCrater), b(m_addCrag), c(m_addPath);
    m_addCrater->setChecked(kind == int(TerrainFeatureKind::Crater));
    m_addCrag->setChecked(kind == int(TerrainFeatureKind::Crag));
    m_addPath->setChecked(kind == int(TerrainFeatureKind::Path));
}

void TerrainPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #d04020;") : QString());
    m_status->setVisible(!text.isEmpty());
}

void TerrainPanel::load()
{
    m_loading = true;
    m_base->setCurrentIndex(int(m_spec.base));
    m_width->setValue(m_spec.width);
    m_depth->setValue(m_spec.depth);
    m_thickness->setValue(m_spec.baseThickness);
    m_edge->setCurrentIndex(int(m_spec.edge));
    m_bevel->setValue(m_spec.bevel);
    m_hillHeight->setValue(m_spec.hillHeight);
    m_hillSize->setValue(m_spec.hillSize);
    m_rough->setValue(int(std::lround(m_spec.roughness * 100)));
    m_roughBox->setValue(int(std::lround(m_spec.roughness * 100)));
    m_levelEdges->setChecked(m_spec.levelEdges);
    m_seed->setValue(int(m_spec.seed));
    m_smallest->setValue(m_spec.smallest);
    m_loading = false;
    fillFeatures();
    showFields();
}

void TerrainPanel::readFields()
{
    m_spec.base = TerrainBase(m_base->currentIndex());
    m_spec.width = m_width->value();
    m_spec.depth = m_depth->value();
    m_spec.baseThickness = m_thickness->value();
    m_spec.edge = TerrainEdge(m_edge->currentIndex());
    m_spec.bevel = m_bevel->value();
    m_spec.hillHeight = m_hillHeight->value();
    m_spec.hillSize = m_hillSize->value();
    m_spec.roughness = m_roughBox->value() / 100.0;
    m_spec.levelEdges = m_levelEdges->isChecked();
    m_spec.seed = std::uint32_t(m_seed->value());
    m_spec.smallest = m_smallest->value();
    preferences().setValue(QStringLiteral("building/smallest"), m_spec.smallest);
}

void TerrainPanel::showFields()
{
    const bool bevelled = m_spec.edge == TerrainEdge::Bevelled;
    m_bevelLabel->setVisible(bevelled);
    m_bevel->setVisible(bevelled);
    const double k = 1.0 / m_spec.scale;
    m_size->setText(tr("On the plate: %1 x %2 mm, hills up to %3 mm above the base, at 1:%4")
                        .arg(m_spec.width * k, 0, 'f', 1)
                        .arg(m_spec.depth * k, 0, 'f', 1)
                        .arg(m_spec.hillHeight * k, 0, 'f', 1)
                        .arg(m_spec.scale));
}

void TerrainPanel::fillFeatures()
{
    const bool was = m_loading;
    m_loading = true;
    m_features->clear();
    for (const TerrainFeature& f : m_spec.features)
        m_features->addItem(tr("%1, %2 mm%3")
                                .arg(tr(terrain::title(f.kind)))
                                .arg(f.size, 0, 'f', 0)
                                .arg(f.scattered ? tr(" (scattered)") : QString()));
    if (m_selected >= int(m_spec.features.size()))
        m_selected = -1;
    m_features->setCurrentRow(m_selected);
    m_loading = was;
    showFeature();
}

void TerrainPanel::showFeature()
{
    const bool any = m_selected >= 0 && m_selected < int(m_spec.features.size());
    m_featureDetails->setVisible(any);
    if (!any)
        return;
    const TerrainFeature& f = m_spec.features[std::size_t(m_selected)];
    const bool was = m_loading;
    m_loading = true;
    switch (f.kind) {
    case TerrainFeatureKind::Crater:
        m_featureSizeLabel->setText(tr("Across:"));
        m_featureDepthLabel->setText(tr("Depth:"));
        m_featureDepth->setToolTip(tr("How deep the crater is"));
        break;
    case TerrainFeatureKind::Crag:
        m_featureSizeLabel->setText(tr("Across:"));
        m_featureDepthLabel->setText(tr("Height:"));
        m_featureDepth->setToolTip(tr("How high the rock stands out of the ground"));
        break;
    case TerrainFeatureKind::Path:
        m_featureSizeLabel->setText(tr("Width:"));
        m_featureDepthLabel->setText(tr("Sunk by:"));
        m_featureDepth->setToolTip(tr("How far the path is worn down into the ground"));
        break;
    }
    m_featureSize->setValue(f.size);
    m_featureDepth->setValue(f.depth);
    m_loading = was;
}

}  // namespace forge::scenery
