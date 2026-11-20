// SPDX-License-Identifier: GPL-3.0-or-later
#include "MannequinPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace forge::app {
namespace {

QSettings preferences()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"), QStringLiteral("Forge3D"));
}

// A label with its combobox right under it (the label resting on top of the box).
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

QDoubleSpinBox* spin(double low, double high, double step, int decimals, const QString& suffix)
{
    auto* box = new QDoubleSpinBox;
    box->setRange(low, high);
    box->setSingleStep(step);
    box->setDecimals(decimals);
    box->setSuffix(suffix);
    return box;
}

// Figure lines by height, in inches (the last entry, "Other", is typed).
constexpr double kSizes[] = {3.75, 5.0, 6.0, 7.0, 12.0};
constexpr int kSizeCount = 5;
// Proportions, in heads.
constexpr double kHeads[] = {6.0, 7.5, 8.0, 9.0};
constexpr int kHeadsCount = 4;

}  // namespace

MannequinPanel::MannequinPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel(tr("Figure Guide"));
    QFont bold = title->font();
    bold.setBold(true);
    title->setFont(bold);
    layout->addWidget(title);
    auto* hint = new QLabel(tr("A see-through mannequin to build a figure to: the right height and proportions, "
                               "posed how you like. It's kept in the file but never printed or exported."));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_add = new QPushButton(tr("Add a Figure Guide"));
    layout->addWidget(m_add);
    connect(m_add, &QPushButton::clicked, this, &MannequinPanel::addRequested);

    m_controls = new QWidget;
    auto* controls = new QVBoxLayout(m_controls);
    controls->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_controls);

    m_shown = new QCheckBox(tr("Show the guide"));
    controls->addWidget(m_shown);

    // Size
    auto* sizeBox = new QGroupBox(tr("Size"));
    auto* sizeLayout = new QVBoxLayout(sizeBox);
    m_size = new QComboBox;
    for (double in : kSizes)
        m_size->addItem(tr("%1 in figure (%2 mm)").arg(in).arg(std::round(in * 25.4)));
    m_size->addItem(tr("Other height"));
    m_size->setToolTip(tr("A figure line's size: the height of a figure standing straight, feet to the top of "
                          "the head"));
    sizeLayout->addWidget(labelled(tr("Figure size:"), m_size));
    auto* sizeForm = new QFormLayout;
    m_height = spin(10.0, 2000.0, 1.0, 1, tr(" mm"));
    m_height->setToolTip(tr("Feet to the top of the head, standing straight"));
    sizeForm->addRow(tr("Height:"), m_height);
    sizeLayout->addLayout(sizeForm);
    m_proportions = new QComboBox;
    m_proportions->addItem(tr("Stylised (6 heads)"));
    m_proportions->addItem(tr("Realistic (7.5 heads)"));
    m_proportions->addItem(tr("Heroic (8 heads)"));
    m_proportions->addItem(tr("Fashion (9 heads)"));
    m_proportions->addItem(tr("Other"));
    m_proportions->setToolTip(tr("How many head-heights tall the figure is: fewer is chunkier and bigger-headed"));
    sizeLayout->addWidget(labelled(tr("Proportions:"), m_proportions));
    auto* headsForm = new QFormLayout;
    m_heads = spin(3.0, 12.0, 0.25, 2, tr(" heads"));
    headsForm->addRow(tr("Heads tall:"), m_heads);
    sizeLayout->addLayout(headsForm);
    m_headSize = new QLabel;
    m_headSize->setStyleSheet(QStringLiteral("color: palette(mid);"));
    sizeLayout->addWidget(m_headSize);
    controls->addWidget(sizeBox);

    // Place
    auto* placeBox = new QGroupBox(tr("Where it stands"));
    auto* placeForm = new QFormLayout(placeBox);
    m_x = spin(-5000.0, 5000.0, 1.0, 1, tr(" mm"));
    m_y = spin(-5000.0, 5000.0, 1.0, 1, tr(" mm"));
    m_turn = spin(-180.0, 180.0, 15.0, 0, tr("°"));
    m_turn->setWrapping(true);
    m_turn->setToolTip(tr("0 faces the front"));
    placeForm->addRow(tr("X:"), m_x);
    placeForm->addRow(tr("Y:"), m_y);
    placeForm->addRow(tr("Turn:"), m_turn);
    controls->addWidget(placeBox);

    // Pose
    auto* poseBox = new QGroupBox(tr("Pose"));
    auto* poseLayout = new QVBoxLayout(poseBox);
    m_preset = new QComboBox;
    for (int p = 0; p < mannequin::kPosePresetCount; ++p)
        m_preset->addItem(tr(mannequin::title(mannequin::PosePreset(p))));
    m_preset->addItem(tr("Your own"));
    m_preset->setToolTip(tr("Start from a ready pose, then change any joint"));
    poseLayout->addWidget(labelled(tr("Pose:"), m_preset));
    m_joint = new QComboBox;
    for (int j = 0; j < kMJointCount; ++j)
        m_joint->addItem(tr(mannequin::title(MJoint(j))));
    poseLayout->addWidget(labelled(tr("Joint:"), m_joint));
    auto* angles = new QFormLayout;
    for (int k = 0; k < 2; ++k) {
        m_angleLabel[k] = new QLabel;
        m_angle[k] = spin(-180.0, 180.0, 5.0, 0, tr("°"));
        angles->addRow(m_angleLabel[k], m_angle[k]);
    }
    poseLayout->addLayout(angles);
    controls->addWidget(poseBox);

    auto* remove = new QPushButton(tr("Remove the Guide"));
    remove->setToolTip(tr("Take the figure guide out of the file (Undo puts it back)"));
    controls->addWidget(remove);
    connect(remove, &QPushButton::clicked, this, &MannequinPanel::removeRequested);

    m_status = new QLabel;
    m_status->setWordWrap(true);
    m_status->hide();
    layout->addWidget(m_status);

    auto* buttons = new QHBoxLayout;
    auto* done = new QPushButton(tr("Done"));
    done->setDefault(true);
    done->setToolTip(tr("Close the panel (Esc); the guide stays"));
    buttons->addStretch(1);
    buttons->addWidget(done);
    layout->addLayout(buttons);
    layout->addStretch(1);
    connect(done, &QPushButton::clicked, this, &MannequinPanel::doneRequested);

    // Changes
    connect(m_shown, &QCheckBox::toggled, this, [this](bool on) {
        if (m_loading || !m_m)
            return;
        m_m->shown = on;
        edited(on ? tr("Show Figure Guide") : tr("Hide Figure Guide"));
    });
    connect(m_size, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (m_loading || !m_m || i >= kSizeCount)
            return;
        m_height->setValue(kSizes[i] * 25.4);  // edits through the height
    });
    connect(m_height, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (m_loading || !m_m)
            return;
        m_m->height = v;
        edited(tr("Size Figure Guide"));
    });
    connect(m_proportions, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (m_loading || !m_m || i >= kHeadsCount)
            return;
        m_heads->setValue(kHeads[i]);
    });
    connect(m_heads, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (m_loading || !m_m)
            return;
        m_m->heads = v;
        edited(tr("Size Figure Guide"));
    });
    for (QDoubleSpinBox* box : {m_x, m_y, m_turn})
        connect(box, &QDoubleSpinBox::valueChanged, this, [this] {
            if (m_loading || !m_m)
                return;
            m_m->x = m_x->value();
            m_m->y = m_y->value();
            m_m->turn = m_turn->value();
            edited(tr("Move Figure Guide"));
        });
    connect(m_preset, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (m_loading || !m_m || i >= mannequin::kPosePresetCount)
            return;
        m_m->pose = mannequin::preset(mannequin::PosePreset(i));
        edited(tr("Pose Figure Guide (%1)").arg(m_preset->currentText()));
    });
    connect(m_joint, &QComboBox::currentIndexChanged, this, &MannequinPanel::showJoint);
    for (int k = 0; k < 2; ++k)
        connect(m_angle[k], &QDoubleSpinBox::valueChanged, this, [this, k](double v) {
            if (m_loading || !m_m)
                return;
            m_m->pose[std::size_t(m_joint->currentIndex())][std::size_t(k)] = v;
            edited(tr("Pose Figure Guide"));
        });

    setMannequin(std::nullopt);
}

Mannequin MannequinPanel::fresh() const
{
    const QSettings prefs = preferences();
    Mannequin m;
    m.height = std::clamp(prefs.value(QStringLiteral("mannequin/height"), m.height).toDouble(), 10.0, 2000.0);
    m.heads = std::clamp(prefs.value(QStringLiteral("mannequin/heads"), m.heads).toDouble(), 3.0, 12.0);
    m.pose = mannequin::preset(mannequin::PosePreset::Standing);
    return m;
}

void MannequinPanel::setMannequin(const std::optional<Mannequin>& m)
{
    m_m = m;
    m_add->setVisible(!m);
    m_controls->setVisible(bool(m));
    if (!m)
        return;
    m_loading = true;
    m_shown->setChecked(m->shown);
    int size = kSizeCount;
    for (int i = 0; i < kSizeCount; ++i)
        if (std::abs(kSizes[i] * 25.4 - m->height) < 0.05)
            size = i;
    m_size->setCurrentIndex(size);
    m_height->setValue(m->height);
    int heads = kHeadsCount;
    for (int i = 0; i < kHeadsCount; ++i)
        if (std::abs(kHeads[i] - m->heads) < 1e-6)
            heads = i;
    m_proportions->setCurrentIndex(heads);
    m_heads->setValue(m->heads);
    m_headSize->setText(tr("A head is %1 mm tall.").arg(m->height / m->heads, 0, 'f', 1));
    m_x->setValue(m->x);
    m_y->setValue(m->y);
    m_turn->setValue(m->turn);
    int preset = mannequin::kPosePresetCount;
    for (int p = 0; p < mannequin::kPosePresetCount; ++p)
        if (mannequin::preset(mannequin::PosePreset(p)) == m->pose)
            preset = p;
    m_preset->setCurrentIndex(preset);
    m_loading = false;
    showJoint();
}

void MannequinPanel::showJoint()
{
    if (!m_m)
        return;
    const auto joint = MJoint(m_joint->currentIndex());
    m_loading = true;
    for (int k = 0; k < 2; ++k) {
        const bool used = k < mannequin::angleCount(joint);
        m_angleLabel[k]->setVisible(used);
        m_angle[k]->setVisible(used);
        if (!used)
            continue;
        m_angleLabel[k]->setText(tr(mannequin::angleTitle(joint, k)) + QLatin1Char(':'));
        const auto range = mannequin::angleRange(joint, k);
        m_angle[k]->setRange(range[0], range[1]);
        m_angle[k]->setValue(m_m->pose[std::size_t(joint)][std::size_t(k)]);
    }
    m_loading = false;
}

void MannequinPanel::edited(const QString& label)
{
    remember();
    emit changed(*m_m, label);
}

void MannequinPanel::remember() const
{
    if (!m_m)
        return;
    QSettings prefs = preferences();
    prefs.setValue(QStringLiteral("mannequin/height"), m_m->height);
    prefs.setValue(QStringLiteral("mannequin/heads"), m_m->heads);
}

void MannequinPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #d04020;") : QString());
    m_status->setVisible(!text.isEmpty());
}

}  // namespace forge::app
