// SPDX-License-Identifier: GPL-3.0-or-later
#include "TreePanel.h"

#include "forge/core/Tree.h"

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

QSpinBox* whole(int lo, int hi, int step, const QString& suffix, const QString& tip)
{
    auto* b = new QSpinBox;
    b->setRange(lo, hi);
    b->setSingleStep(step);
    b->setSuffix(suffix);
    b->setKeyboardTracking(false);
    b->setToolTip(tip);
    return b;
}

}  // namespace

TreePanel::TreePanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    layout->addWidget(m_title);
    layout->addWidget(wrapped(tr("Sizes are real-world (millimetres); the tree is made at the model scale, standing on "
                                 "flared roots cut flat to print on.")));

    auto* treeBox = new QGroupBox(tr("Tree"));
    auto* tree = new QVBoxLayout(treeBox);
    m_kind = new QComboBox;
    for (TreeKind k : {TreeKind::Broadleaf, TreeKind::Conifer, TreeKind::Dead, TreeKind::Palm})
        m_kind->addItem(tr(tree::title(k)));
    m_kind->setToolTip(tr("Broadleaf: spreading branches, a rounded crown. Conifer: a straight trunk, tiers of "
                          "drooping branches. Dead: bare, gnarled branches. Palm: a curved trunk, fronds at the top"));
    auto* kindColumn = new QVBoxLayout;
    kindColumn->setSpacing(0);
    kindColumn->addWidget(new QLabel(tr("Kind:")));
    kindColumn->addWidget(m_kind);
    tree->addLayout(kindColumn);
    auto* form = new QFormLayout;
    const QString real = tr(" mm");
    m_height = spin(300, 150000, 0, 500, real, tr("From the ground to the top of the crown"));
    form->addRow(tr("Height:"), m_height);
    m_crown = spin(200, 100000, 0, 250, real, tr("Across the crown (a palm's: across its fronds)"));
    form->addRow(tr("Crown width:"), m_crown);
    m_trunk = spin(20, 10000, 0, 25, real, tr("The trunk's thickness at the bottom, above the roots"));
    form->addRow(tr("Trunk diameter:"), m_trunk);
    m_branches = whole(0, 100, 10, tr(" %"), tr("How many branches (a palm's: fronds; a conifer's: tiers)"));
    form->addRow(tr("Branches:"), m_branches);
    m_lean = whole(0, 30, 1, tr(" °"), tr("How far the tree leans from upright"));
    form->addRow(tr("Lean:"), m_lean);
    tree->addLayout(form);
    m_leaves = new QCheckBox(tr("Leaves (foliage clumps)"));
    m_leaves->setToolTip(tr("Lumpy clumps of foliage on the branches; untick for a bare tree"));
    tree->addWidget(m_leaves);
    auto* leafForm = new QFormLayout;
    m_foliage = whole(0, 100, 10, tr(" %"), tr("How big and full the clumps are"));
    m_foliageLabel = new QLabel(tr("Foliage:"));
    leafForm->addRow(m_foliageLabel, m_foliage);
    tree->addLayout(leafForm);
    layout->addWidget(treeBox);

    auto* shapeBox = new QGroupBox(tr("Which one"));
    auto* shape = new QVBoxLayout(shapeBox);
    shape->addWidget(wrapped(tr("Every tree is different: New Shape makes another of the same kind and size.")));
    auto* seedRow = new QHBoxLayout;
    auto* another = new QPushButton(tr("New Shape"));
    another->setToolTip(tr("Another tree like this one"));
    m_seed = whole(1, 999999, 1, {}, tr("Its number: the same number makes the same tree again"));
    seedRow->addWidget(another);
    seedRow->addWidget(new QLabel(tr("Number:")));
    seedRow->addWidget(m_seed);
    shape->addLayout(seedRow);
    layout->addWidget(shapeBox);

    auto* printForm = new QFormLayout;
    m_smallest = spin(0.05, 2.0, 2, 0.05, tr(" mm"),
                      tr("The finest detail your printer makes: thin branches are thickened to print (at least "
                         "2.5 times this, and 0.8 mm)"));
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
    for (QDoubleSpinBox* b : {m_height, m_crown, m_trunk, m_smallest})
        connect(b, &QDoubleSpinBox::valueChanged, this, changedHere);
    for (QSpinBox* b : {m_branches, m_foliage, m_lean, m_seed})
        connect(b, &QSpinBox::valueChanged, this, changedHere);
    connect(m_leaves, &QCheckBox::toggled, this, changedHere);
    connect(m_kind, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (m_loading)
            return;
        readFields();
        m_spec = tree::defaults(TreeKind(i), m_spec);
        fitPrinter();
        load();
        emit changed();
    });
    connect(another, &QPushButton::clicked, this,
            [this] { m_seed->setValue(int(QRandomGenerator::global()->bounded(1, 999999))); });
    for (QWidget* w : findChildren<QAbstractSpinBox*>()) {
        w->setFocusPolicy(Qt::StrongFocus);
        w->installEventFilter(this);
    }
    for (QWidget* w : findChildren<QComboBox*>()) {
        w->setFocusPolicy(Qt::StrongFocus);
        w->installEventFilter(this);
    }
    connect(m_apply, &QPushButton::clicked, this, &TreePanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &TreePanel::cancelRequested);
}

bool TreePanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Wheel) {
        if (auto* w = qobject_cast<QWidget*>(watched); w && !w->hasFocus()) {
            event->ignore();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TreePanel::start(const TreeSpec& spec, bool editing)
{
    m_spec = spec;
    if (!editing) {
        m_spec.smallest = preferences().value(QStringLiteral("building/smallest"), 0.4).toDouble();
        m_spec.seed = std::uint32_t(QRandomGenerator::global()->bounded(1, 999999));
        fitPrinter();
    }
    m_title->setText(editing ? tr("Change the tree") : tr("New tree"));
    m_apply->setText(editing ? tr("Update Tree") : tr("Add Tree"));
    load();
    setStatus({}, false);
}

void TreePanel::setPrinter(double height, double width)
{
    m_printHeight = height;
    m_printWidth = width;
}

// Its usual size, made smaller (all of it alike) if it wouldn't fit the printer at this scale.
void TreePanel::fitPrinter()
{
    const double k = 1.0 / m_spec.scale;
    const double fits = std::min({m_printHeight * 0.85 / (m_spec.height * k), m_printWidth * 0.85 / (m_spec.crownWidth * k), 1.0});
    if (!(fits < 1.0))
        return;
    m_spec.height = std::max(300.0, std::floor(m_spec.height * fits / 100.0) * 100.0);
    m_spec.crownWidth = std::max(200.0, std::floor(m_spec.crownWidth * fits / 100.0) * 100.0);
    m_spec.trunkDiameter = std::max(20.0, std::floor(m_spec.trunkDiameter * fits / 10.0) * 10.0);
}

void TreePanel::setSpec(const TreeSpec& spec)
{
    m_spec = spec;
    load();
}

void TreePanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #d04020;") : QString());
    m_status->setVisible(!text.isEmpty());
}

void TreePanel::load()
{
    m_loading = true;
    m_kind->setCurrentIndex(int(m_spec.kind));
    m_height->setValue(m_spec.height);
    m_crown->setValue(m_spec.crownWidth);
    m_trunk->setValue(m_spec.trunkDiameter);
    m_branches->setValue(int(std::lround(m_spec.branches * 100)));
    m_leaves->setChecked(m_spec.leaves);
    m_foliage->setValue(int(std::lround(m_spec.foliage * 100)));
    m_lean->setValue(int(std::lround(m_spec.lean)));
    m_seed->setValue(int(m_spec.seed));
    m_smallest->setValue(m_spec.smallest);
    m_loading = false;
    showKind();
}

void TreePanel::readFields()
{
    m_spec.kind = TreeKind(m_kind->currentIndex());
    m_spec.height = m_height->value();
    m_spec.crownWidth = m_crown->value();
    m_spec.trunkDiameter = m_trunk->value();
    m_spec.branches = m_branches->value() / 100.0;
    m_spec.leaves = m_leaves->isChecked();
    m_spec.foliage = m_foliage->value() / 100.0;
    m_spec.lean = m_lean->value();
    m_spec.seed = std::uint32_t(m_seed->value());
    m_spec.smallest = m_smallest->value();
    preferences().setValue(QStringLiteral("building/smallest"), m_spec.smallest);
}

void TreePanel::showKind()
{
    const bool dead = m_spec.kind == TreeKind::Dead;
    m_leaves->setVisible(!dead);
    const bool leafy = m_spec.leaves && !dead;
    m_foliageLabel->setVisible(leafy);
    m_foliage->setVisible(leafy);
    const double k = 1.0 / m_spec.scale;
    m_size->setText(tr("On the plate: about %1 mm tall, %2 mm across, at 1:%3. Branches thinner than %4 mm are "
                       "thickened to print.")
                        .arg(m_spec.height * k, 0, 'f', 1)
                        .arg(m_spec.crownWidth * k, 0, 'f', 1)
                        .arg(m_spec.scale)
                        .arg(tree::thinnest(m_spec), 0, 'f', 1));
}

}  // namespace forge::scenery
