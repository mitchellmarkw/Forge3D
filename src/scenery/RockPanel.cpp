// SPDX-License-Identifier: GPL-3.0-or-later
#include "RockPanel.h"

#include "forge/core/Rock.h"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
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

}  // namespace

RockPanel::RockPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    layout->addWidget(m_title);
    layout->addWidget(wrapped(tr("Sizes are real-world (millimetres); the rock is made at the model scale, sunk into "
                                 "the ground with a flat base to print on.")));

    auto* rockBox = new QGroupBox(tr("Rock"));
    auto* rock = new QVBoxLayout(rockBox);
    // The kind, its label resting on its list.
    m_kind = new QComboBox;
    for (RockKind k : {RockKind::Boulder, RockKind::Crag, RockKind::Slab, RockKind::Pile})
        m_kind->addItem(tr(rock::title(k)));
    m_kind->setToolTip(tr("Boulder: rounded and worn. Crag: broken, flat faces and sharp edges. Slab: a flat stone. "
                          "Pile: a heap of smaller rocks"));
    auto* kindColumn = new QVBoxLayout;
    kindColumn->setSpacing(0);
    kindColumn->addWidget(new QLabel(tr("Kind:")));
    kindColumn->addWidget(m_kind);
    rock->addLayout(kindColumn);
    auto* form = new QFormLayout;
    const QString real = tr(" mm");
    m_length = spin(10, 200000, 0, 50, real, tr("Its length, left to right"));
    form->addRow(tr("Length:"), m_length);
    m_width = spin(10, 200000, 0, 50, real, tr("Its width, front to back"));
    form->addRow(tr("Width:"), m_width);
    m_height = spin(10, 200000, 0, 50, real, tr("Its height, before it's sunk into the ground"));
    form->addRow(tr("Height:"), m_height);
    auto* roughRow = new QHBoxLayout;
    m_rough = new QSlider(Qt::Horizontal);
    m_rough->setRange(0, 100);
    m_roughBox = new QSpinBox;
    m_roughBox->setRange(0, 100);
    m_roughBox->setSuffix(tr(" %"));
    m_roughBox->setKeyboardTracking(false);
    m_roughBox->setToolTip(tr("How lumpy and rough its surface is"));
    roughRow->addWidget(m_rough, 1);
    roughRow->addWidget(m_roughBox);
    form->addRow(tr("Roughness:"), roughRow);
    m_sunk = new QSpinBox;
    m_sunk->setRange(0, 80);
    m_sunk->setSuffix(tr(" %"));
    m_sunk->setKeyboardTracking(false);
    m_sunk->setToolTip(tr("How much of its height is in the ground: its base is cut flat there"));
    form->addRow(tr("Sunk into the ground:"), m_sunk);
    m_count = new QSpinBox;
    m_count->setRange(2, 40);
    m_count->setKeyboardTracking(false);
    m_count->setToolTip(tr("How many rocks are in the pile"));
    m_countLabel = new QLabel(tr("Rocks in the pile:"));
    form->addRow(m_countLabel, m_count);
    rock->addLayout(form);
    m_layers = new QCheckBox(tr("Layers (bedding lines across it)"));
    m_layers->setToolTip(tr("Grooves across the rock a layer apart, as in sandstone or shale"));
    rock->addWidget(m_layers);
    auto* layerForm = new QFormLayout;
    m_layerThickness = spin(5, 20000, 0, 10, real, tr("How far apart the layers are"));
    m_layerLabel = new QLabel(tr("Layer thickness:"));
    layerForm->addRow(m_layerLabel, m_layerThickness);
    rock->addLayout(layerForm);
    layout->addWidget(rockBox);

    auto* shapeBox = new QGroupBox(tr("Which one"));
    auto* shape = new QVBoxLayout(shapeBox);
    shape->addWidget(wrapped(tr("Every rock is different: New Shape makes another of the same kind and size.")));
    auto* seedRow = new QHBoxLayout;
    auto* another = new QPushButton(tr("New Shape"));
    another->setToolTip(tr("Another rock like this one"));
    m_seed = new QSpinBox;
    m_seed->setRange(1, 999999);
    m_seed->setKeyboardTracking(false);
    m_seed->setToolTip(tr("Its number: the same number makes the same rock again"));
    seedRow->addWidget(another);
    seedRow->addWidget(new QLabel(tr("Number:")));
    seedRow->addWidget(m_seed);
    shape->addLayout(seedRow);
    layout->addWidget(shapeBox);

    auto* printForm = new QFormLayout;
    m_smallest = spin(0.05, 2.0, 2, 0.05, tr(" mm"), tr("The finest detail your printer makes: no finer surface is made"));
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
        showKind();
        emit changed();
    };
    for (QDoubleSpinBox* b : {m_length, m_width, m_height, m_layerThickness, m_smallest})
        connect(b, &QDoubleSpinBox::valueChanged, this, changedHere);
    for (QSpinBox* b : {m_roughBox, m_sunk, m_count, m_seed})
        connect(b, &QSpinBox::valueChanged, this, changedHere);
    connect(m_layers, &QCheckBox::toggled, this, changedHere);
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
    connect(m_kind, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (m_loading)
            return;
        readFields();
        m_spec = rock::defaults(RockKind(i), m_spec);
        load();
        emit changed();
    });
    connect(another, &QPushButton::clicked, this, [this] {
        m_seed->setValue(int(QRandomGenerator::global()->bounded(1, 999999)));
    });
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
    connect(m_apply, &QPushButton::clicked, this, &RockPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &RockPanel::cancelRequested);
}

bool RockPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Wheel) {
        if (auto* w = qobject_cast<QWidget*>(watched); w && !w->hasFocus()) {
            event->ignore();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void RockPanel::start(const RockSpec& spec, bool editing)
{
    m_spec = spec;
    if (!editing) {
        m_spec.smallest = preferences().value(QStringLiteral("building/smallest"), 0.4).toDouble();
        m_spec.seed = std::uint32_t(QRandomGenerator::global()->bounded(1, 999999));
    }
    m_title->setText(editing ? tr("Change the rock") : tr("New rock"));
    m_apply->setText(editing ? tr("Update Rock") : tr("Add Rock"));
    load();
    setStatus({}, false);
}

void RockPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #d04020;") : QString());
    m_status->setVisible(!text.isEmpty());
}

void RockPanel::load()
{
    m_loading = true;
    m_kind->setCurrentIndex(int(m_spec.kind));
    m_length->setValue(m_spec.length);
    m_width->setValue(m_spec.width);
    m_height->setValue(m_spec.height);
    m_rough->setValue(int(std::lround(m_spec.roughness * 100)));
    m_roughBox->setValue(int(std::lround(m_spec.roughness * 100)));
    m_layers->setChecked(m_spec.layers);
    m_layerThickness->setValue(m_spec.layerThickness);
    m_sunk->setValue(int(std::lround(m_spec.sunk * 100)));
    m_count->setValue(m_spec.count);
    m_seed->setValue(int(m_spec.seed));
    m_smallest->setValue(m_spec.smallest);
    m_loading = false;
    showKind();
}

void RockPanel::readFields()
{
    m_spec.kind = RockKind(m_kind->currentIndex());
    m_spec.length = m_length->value();
    m_spec.width = m_width->value();
    m_spec.height = m_height->value();
    m_spec.roughness = m_roughBox->value() / 100.0;
    m_spec.layers = m_layers->isChecked();
    m_spec.layerThickness = m_layerThickness->value();
    m_spec.sunk = m_sunk->value() / 100.0;
    m_spec.count = m_count->value();
    m_spec.seed = std::uint32_t(m_seed->value());
    m_spec.smallest = m_smallest->value();
    preferences().setValue(QStringLiteral("building/smallest"), m_spec.smallest);
}

void RockPanel::showKind()
{
    const bool pile = m_spec.kind == RockKind::Pile;
    m_countLabel->setVisible(pile);
    m_count->setVisible(pile);
    m_layerLabel->setVisible(m_spec.layers);
    m_layerThickness->setVisible(m_spec.layers);
    const double k = 1.0 / m_spec.scale;
    m_size->setText(tr("On the plate: about %1 x %2 x %3 mm, at 1:%4")
                        .arg(m_spec.length * k, 0, 'f', 1)
                        .arg(m_spec.width * k, 0, 'f', 1)
                        .arg(m_spec.height * (1.0 - m_spec.sunk) * k, 0, 'f', 1)
                        .arg(m_spec.scale));
}

}  // namespace forge::scenery
