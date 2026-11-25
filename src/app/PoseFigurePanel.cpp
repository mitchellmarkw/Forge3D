// SPDX-License-Identifier: GPL-3.0-or-later
#include "PoseFigurePanel.h"

#include "forge/core/Mannequin.h"
#include "forge/core/Rig.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
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

QLabel* wrapped(const QString& text)
{
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    return label;
}

}  // namespace

PoseFigurePanel::PoseFigurePanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    m_title->setWordWrap(true);
    layout->addWidget(m_title);
    layout->addWidget(wrapped(tr("Bends a figure sculpted in one piece at its joints, the surface following. Best "
                                 "on a figure standing straight, facing the front, with its arms away from its "
                                 "sides (an A or T pose).")));

    // The stages, two to a row (so their names fit).
    auto* stages = new QGridLayout;
    m_fitStage = new QPushButton(tr("1. Fit the Skeleton"));
    m_poseStage = new QPushButton(tr("2. Pose"));
    m_sizeStage = new QPushButton(tr("3. Proportions"));
    m_styleStage = new QPushButton(tr("4. Style"));
    auto* group = new QButtonGroup(this);
    int cell = 0;
    for (QPushButton* b : {m_fitStage, m_poseStage, m_sizeStage, m_styleStage}) {
        b->setCheckable(true);
        group->addButton(b);
        stages->addWidget(b, cell / 2, cell % 2);
        ++cell;
    }
    group->setExclusive(true);
    layout->addLayout(stages);

    // Fitting.
    m_fitBox = new QGroupBox(tr("Fit the skeleton"));
    auto* fit = new QVBoxLayout(m_fitBox);
    fit->addWidget(wrapped(tr("Drag the balls onto the figure's joints: the middle of each elbow, wrist, knee and "
                              "ankle, and the hips. Look from the front, then from the side.")));
    m_mirror = new QCheckBox(tr("Move both sides together"));
    m_mirror->setToolTip(tr("Dragging a ball on one side moves its partner on the other side the same way"));
    fit->addWidget(m_mirror);
    auto* blendForm = new QFormLayout;
    m_blend = new QDoubleSpinBox;
    m_blend->setRange(0.1, 200.0);
    m_blend->setDecimals(1);
    m_blend->setSingleStep(0.5);
    m_blend->setSuffix(tr(" mm"));
    m_blend->setKeyboardTracking(false);
    m_blend->setToolTip(tr("How far a bend spreads either side of a joint: more is softer and rounder, less keeps "
                           "the bend tight to the joint"));
    blendForm->addRow(tr("Bends spread:"), m_blend);
    fit->addLayout(blendForm);
    auto* again = new QPushButton(tr("Place It Again"));
    again->setToolTip(tr("Put the skeleton back where Forge3D first placed it in the figure"));
    fit->addWidget(again);
    layout->addWidget(m_fitBox);

    // Posing.
    m_poseBox = new QGroupBox(tr("Pose"));
    auto* pose = new QVBoxLayout(m_poseBox);
    pose->addWidget(wrapped(tr("Drag a ball to bend the joint above it, or choose a joint and type its angles.")));
    m_preset = new QComboBox;
    m_preset->setToolTip(tr("Start from a ready pose (the Figure Guide's), then change any joint"));
    pose->addWidget(labelled(tr("Pose:"), m_preset));
    m_joint = new QComboBox;
    for (int j = 0; j < kRigJointCount; ++j)
        m_joint->addItem(tr(rig::title(rig::Joint(j))));
    pose->addWidget(labelled(tr("Joint:"), m_joint));
    auto* angles = new QFormLayout;
    for (int k = 0; k < 2; ++k) {
        m_angleLabel[k] = new QLabel;
        m_angle[k] = new QDoubleSpinBox;
        m_angle[k]->setDecimals(1);
        m_angle[k]->setSingleStep(5.0);
        m_angle[k]->setSuffix(tr("°"));
        m_angle[k]->setKeyboardTracking(false);
        angles->addRow(m_angleLabel[k], m_angle[k]);
    }
    pose->addLayout(angles);
    m_feetFlat = new QCheckBox(tr("Feet flat on the ground"));
    m_feetFlat->setToolTip(tr("The feet keep their angle as the legs bend (the ankles' angles aren't used)"));
    pose->addWidget(m_feetFlat);
    m_onGround = new QCheckBox(tr("Keep it standing on the plate"));
    m_onGround->setToolTip(tr("Moved up or down so its lowest point stays where it was (bent legs bring it down)"));
    pose->addWidget(m_onGround);
    layout->addWidget(m_poseBox);

    // Proportions.
    m_sizeBox = new QGroupBox(tr("Proportions"));
    auto* sizes = new QVBoxLayout(m_sizeBox);
    sizes->addWidget(wrapped(tr("Make parts bigger or smaller, longer or shorter: 100% is as made. The pose goes "
                                "on from the new proportions.")));
    m_sizePreset = new QComboBox;
    m_sizePreset->addItem(tr("As it was made"));
    m_sizePreset->addItem(tr("Heroic (long legs, broad shoulders, a smaller head)"));
    m_sizePreset->addItem(tr("Chibi (a big head on a small body)"));
    m_sizePreset->addItem(tr("Your own"));
    sizes->addWidget(labelled(tr("Ready proportions:"), m_sizePreset));
    auto* sizeForm = new QFormLayout;
    for (int i = 0; i < kProportionCount; ++i) {
        auto* row = new QWidget;
        auto* line = new QHBoxLayout(row);
        line->setContentsMargins(0, 0, 0, 0);
        m_sizeSlider[i] = new QSlider(Qt::Horizontal);
        m_sizeSlider[i]->setRange(int(rig::kLeastProportion * 100), int(rig::kMostProportion * 100));
        m_sizeSlider[i]->setSingleStep(5);
        m_sizeSlider[i]->setPageStep(10);
        m_sizePercent[i] = new QSpinBox;
        m_sizePercent[i]->setRange(int(rig::kLeastProportion * 100), int(rig::kMostProportion * 100));
        m_sizePercent[i]->setSingleStep(5);
        m_sizePercent[i]->setSuffix(tr(" %"));
        m_sizePercent[i]->setKeyboardTracking(false);
        line->addWidget(m_sizeSlider[i], 1);
        line->addWidget(m_sizePercent[i]);
        sizeForm->addRow(tr(rig::title(rig::Proportion(i))) + tr(":"), row);
    }
    sizes->addLayout(sizeForm);
    layout->addWidget(m_sizeBox);
    m_sizeBox->hide();

    // Figure styles.
    m_styleBox = new QGroupBox(tr("Style"));
    auto* style = new QVBoxLayout(m_styleBox);
    style->addWidget(wrapped(tr("Turn the figure into a toy style in one go. A style sets the proportions (fine-tune "
                                "them in 3. Proportions); the pose goes on from it.")));
    m_style = new QComboBox;
    m_style->addItem(tr("None (as it was made)"));
    for (int s = 0; s < figurestyle::kStyleCount; ++s)
        m_style->addItem(tr(figurestyle::title(figurestyle::Style(s))));
    m_style->setToolTip(tr("The look to give the figure"));
    style->addWidget(labelled(tr("Figure style:"), m_style));
    m_styleBlurb = wrapped({});
    style->addWidget(m_styleBlurb);
    const auto sliderRow = [](QSlider*& slider, QSpinBox*& box, int most) {
        auto* row = new QWidget;
        auto* line = new QHBoxLayout(row);
        line->setContentsMargins(0, 0, 0, 0);
        slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, most);
        slider->setSingleStep(5);
        slider->setPageStep(10);
        box = new QSpinBox;
        box->setRange(0, most);
        box->setSingleStep(5);
        box->setSuffix(QStringLiteral(" %"));
        box->setKeyboardTracking(false);
        line->addWidget(slider, 1);
        line->addWidget(box);
        return row;
    };
    auto* styleForm = new QFormLayout;
    styleForm->addRow(tr("Strength:"), sliderRow(m_strengthSlider, m_strength, int(figurestyle::kMostStrength * 100)));
    m_strength->setToolTip(tr("How far to take the style: 100% is the style itself, less is nearer the figure as "
                              "made, more goes further"));
    styleForm->addRow(tr("Blocky:"), sliderRow(m_blockySlider, m_blocky, 100));
    m_blocky->setToolTip(tr("How square the figure's round parts are made, about each bone: 0% as they are, 100% "
                            "squared off"));
    style->addLayout(styleForm);
    m_bobbleBox = new QGroupBox(tr("Bobblehead spring"));
    auto* bobble = new QVBoxLayout(m_bobbleBox);
    m_separate = new QCheckBox(tr("Cut the head off to sit on a spring"));
    m_separate->setToolTip(tr("On Apply, the head is cut off at the neck as a part of its own, with a socket in it "
                              "for a spring, and a post on the neck for the spring's other end"));
    bobble->addWidget(m_separate);
    auto* springForm = new QFormLayout;
    m_springSize = new QDoubleSpinBox;
    m_springSize->setRange(figurestyle::kLeastSpring, figurestyle::kMostSpring);
    m_springSize->setDecimals(1);
    m_springSize->setSingleStep(0.5);
    m_springSize->setSuffix(tr(" mm"));
    m_springSize->setValue(5.0);
    m_springSize->setToolTip(tr("How wide the spring is, outside (measure the spring you'll use)"));
    springForm->addRow(tr("Spring diameter:"), m_springSize);
    m_springWire = new QDoubleSpinBox;
    m_springWire->setRange(0.1, 5.0);
    m_springWire->setDecimals(2);
    m_springWire->setSingleStep(0.05);
    m_springWire->setSuffix(tr(" mm"));
    m_springWire->setValue(0.5);
    m_springWire->setToolTip(tr("How thick the spring's wire is: the post fits inside the spring"));
    springForm->addRow(tr("Wire thickness:"), m_springWire);
    m_springFit = new QDoubleSpinBox;
    m_springFit->setRange(0.0, 1.0);
    m_springFit->setDecimals(2);
    m_springFit->setSingleStep(0.05);
    m_springFit->setSuffix(tr(" mm"));
    m_springFit->setValue(0.2);
    m_springFit->setToolTip(tr("Room left round the spring in the head's socket"));
    springForm->addRow(tr("Fit:"), m_springFit);
    bobble->addLayout(springForm);
    style->addWidget(m_bobbleBox);
    layout->addWidget(m_styleBox);
    m_styleBox->hide();

    m_status = new QLabel;
    m_status->setWordWrap(true);
    m_status->hide();
    layout->addWidget(m_status);

    auto* buttons = new QHBoxLayout;
    auto* cancel = new QPushButton(tr("Cancel"));
    auto* apply = new QPushButton(tr("Apply"));
    apply->setDefault(true);
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(apply);
    layout->addLayout(buttons);
    layout->addStretch(1);

    m_mirror->setChecked(preferences().value(QStringLiteral("poseFigure/mirror"), true).toBool());
    connect(m_mirror, &QCheckBox::toggled, this,
            [](bool on) { preferences().setValue(QStringLiteral("poseFigure/mirror"), on); });

    connect(group, &QButtonGroup::buttonToggled, this, [this](QAbstractButton* b, bool on) {
        if (!on)
            return;
        const bool posing = b != m_fitStage;  // posing or proportions: the skeleton is set
        m_fitBox->setVisible(!posing);
        m_poseBox->setVisible(b == m_poseStage);
        m_sizeBox->setVisible(b == m_sizeStage);
        m_styleBox->setVisible(b == m_styleStage);
        if (!m_loading)
            emit stageChanged(posing);
    });
    connect(again, &QPushButton::clicked, this, &PoseFigurePanel::fitAgainRequested);
    connect(m_blend, &QDoubleSpinBox::valueChanged, this, [this] {
        if (!m_loading)
            emit optionsChanged();
    });
    for (QCheckBox* box : {m_feetFlat, m_onGround})
        connect(box, &QCheckBox::toggled, this, [this] {
            if (m_loading)
                return;
            showJoint();
            emit optionsChanged();
        });
    connect(m_preset, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (!m_loading && i < m_preset->count() - 1)
            emit presetChosen(i);
    });
    connect(m_joint, &QComboBox::currentIndexChanged, this, [this](int j) {
        showJoint();
        if (!m_loading)
            emit jointChosen(j);
    });
    for (int k = 0; k < 2; ++k)
        connect(m_angle[k], &QDoubleSpinBox::valueChanged, this, [this, k](double v) {
            if (!m_loading)
                emit angleChanged(m_joint->currentIndex(), k, v);
        });
    for (int i = 0; i < kProportionCount; ++i) {
        // Dragging shows the number; letting go (or a click, a key, the box) changes the figure.
        connect(m_sizeSlider[i], &QSlider::valueChanged, this, [this, i](int v) {
            const QSignalBlocker block(m_sizePercent[i]);
            m_sizePercent[i]->setValue(v);
            if (!m_loading && !m_sizeSlider[i]->isSliderDown()) {
                showSizePreset();
                emit proportionsChanged();
            }
        });
        connect(m_sizeSlider[i], &QSlider::sliderReleased, this, [this] {
            showSizePreset();
            emit proportionsChanged();
        });
        connect(m_sizePercent[i], &QSpinBox::valueChanged, this, [this, i](int v) {
            const QSignalBlocker block(m_sizeSlider[i]);
            m_sizeSlider[i]->setValue(v);
            if (!m_loading) {
                showSizePreset();
                emit proportionsChanged();
            }
        });
    }
    connect(m_sizePreset, &QComboBox::activated, this, [this](int i) {
        if (i >= 3)
            return;  // your own: as they are
        const rig::Proportions p = i == 1 ? rig::heroic() : i == 2 ? rig::chibi() : rig::asMade();
        m_loading = true;
        for (int k = 0; k < kProportionCount; ++k) {
            m_sizeSlider[k]->setValue(int(std::lround(p[std::size_t(k)] * 100)));
            m_sizePercent[k]->setValue(int(std::lround(p[std::size_t(k)] * 100)));
        }
        m_loading = false;
        emit proportionsChanged();
    });
    {
        const auto s = preferences();
        m_strength->setValue(s.value(QStringLiteral("poseFigure/styleStrength"), 100).toInt());
        m_strengthSlider->setValue(m_strength->value());
        m_separate->setChecked(s.value(QStringLiteral("poseFigure/separateHead"), true).toBool());
        m_springSize->setValue(s.value(QStringLiteral("poseFigure/springSize"), 5.0).toDouble());
        m_springWire->setValue(s.value(QStringLiteral("poseFigure/springWire"), 0.5).toDouble());
        m_springFit->setValue(s.value(QStringLiteral("poseFigure/springFit"), 0.2).toDouble());
    }
    connect(m_style, &QComboBox::currentIndexChanged, this, [this] { showStyleOptions(); });
    connect(m_style, &QComboBox::activated, this, [this] { useStyle(); });  // None again: back to as made
    connect(m_strengthSlider, &QSlider::valueChanged, this, [this](int v) {
        const QSignalBlocker block(m_strength);
        m_strength->setValue(v);
        if (!m_loading && !m_strengthSlider->isSliderDown())
            useStyle();
    });
    connect(m_strengthSlider, &QSlider::sliderReleased, this, [this] { useStyle(); });
    connect(m_strength, &QSpinBox::valueChanged, this, [this](int v) {
        const QSignalBlocker block(m_strengthSlider);
        m_strengthSlider->setValue(v);
        preferences().setValue(QStringLiteral("poseFigure/styleStrength"), v);
        if (!m_loading)
            useStyle();
    });
    connect(m_blockySlider, &QSlider::valueChanged, this, [this](int v) {
        const QSignalBlocker block(m_blocky);
        m_blocky->setValue(v);
        if (!m_loading && !m_blockySlider->isSliderDown())
            emit proportionsChanged();
    });
    connect(m_blockySlider, &QSlider::sliderReleased, this, [this] { emit proportionsChanged(); });
    connect(m_blocky, &QSpinBox::valueChanged, this, [this](int v) {
        const QSignalBlocker block(m_blockySlider);
        m_blockySlider->setValue(v);
        if (!m_loading)
            emit proportionsChanged();
    });
    connect(m_separate, &QCheckBox::toggled, this, [this](bool on) {
        preferences().setValue(QStringLiteral("poseFigure/separateHead"), on);
        showStyleOptions();
    });
    connect(m_springSize, &QDoubleSpinBox::valueChanged, this,
            [](double v) { preferences().setValue(QStringLiteral("poseFigure/springSize"), v); });
    connect(m_springWire, &QDoubleSpinBox::valueChanged, this,
            [](double v) { preferences().setValue(QStringLiteral("poseFigure/springWire"), v); });
    connect(m_springFit, &QDoubleSpinBox::valueChanged, this,
            [](double v) { preferences().setValue(QStringLiteral("poseFigure/springFit"), v); });
    showStyleOptions();
    connect(apply, &QPushButton::clicked, this, &PoseFigurePanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &PoseFigurePanel::cancelRequested);
}

void PoseFigurePanel::showProportions()
{
    m_sizeStage->setChecked(true);
}

void PoseFigurePanel::showStyle()
{
    m_styleStage->setChecked(true);
}

double PoseFigurePanel::blocky() const
{
    return m_blocky->value() / 100.0;
}

int PoseFigurePanel::style() const
{
    return m_style->currentIndex() - 1;
}

bool PoseFigurePanel::separateHead() const
{
    return style() == int(figurestyle::Style::Bobblehead) && m_separate->isChecked();
}

figurestyle::Spring PoseFigurePanel::spring() const
{
    figurestyle::Spring s;
    s.diameter = m_springSize->value();
    s.wire = m_springWire->value();
    s.fit = m_springFit->value();
    return s;
}

void PoseFigurePanel::showStyleOptions()
{
    const int s = style();
    m_styleBlurb->setText(s < 0 ? (rig::shaped(m_f) ? tr("Proportions of your own: choose a style to replace them, "
                                                         "or None to go back to the figure as made.")
                                                      : tr("The figure's own proportions; nothing squared off."))
                                : tr(figurestyle::blurb(figurestyle::Style(s))));
    m_strength->setEnabled(s >= 0);
    m_strengthSlider->setEnabled(s >= 0);
    m_bobbleBox->setVisible(s == int(figurestyle::Style::Bobblehead));
    for (QWidget* w : {static_cast<QWidget*>(m_springSize), static_cast<QWidget*>(m_springWire),
                       static_cast<QWidget*>(m_springFit)})
        w->setEnabled(m_separate->isChecked());
}

// The style's proportions and blockiness, from the skeleton as fitted.
void PoseFigurePanel::useStyle()
{
    const int s = style();
    const figurestyle::Look look = s < 0 ? figurestyle::Look{}
                                         : figurestyle::look(figurestyle::Style(s), m_f.points, m_strength->value() / 100.0);
    m_loading = true;
    for (int k = 0; k < kProportionCount; ++k) {
        const int v = int(std::lround(look.proportions[std::size_t(k)] * 100));
        m_sizeSlider[k]->setValue(v);
        m_sizePercent[k]->setValue(v);
    }
    m_blockySlider->setValue(int(std::lround(look.blocky * 100)));
    m_blocky->setValue(int(std::lround(look.blocky * 100)));
    m_loading = false;
    showSizePreset();
    emit proportionsChanged();
}

rig::Proportions PoseFigurePanel::proportions() const
{
    rig::Proportions p{};
    for (int i = 0; i < kProportionCount; ++i)
        p[std::size_t(i)] = m_sizePercent[i]->value() / 100.0;
    return p;
}

void PoseFigurePanel::showSizePreset()
{
    const rig::Proportions now = proportions();
    const auto same = [&](const rig::Proportions& p) {
        for (int i = 0; i < kProportionCount; ++i)
            if (std::abs(p[std::size_t(i)] - now[std::size_t(i)]) > 0.004)
                return false;
        return true;
    };
    const QSignalBlocker block(m_sizePreset);
    m_sizePreset->setCurrentIndex(same(rig::asMade()) ? 0 : same(rig::heroic()) ? 1 : same(rig::chibi()) ? 2 : 3);
}

void PoseFigurePanel::start(const QString& title, bool editing, const PosedFinish& f, bool guide)
{
    m_title->setText(title);
    m_guide = guide;
    m_loading = true;
    m_preset->clear();
    m_preset->addItem(tr("As it was made"));
    for (int p = 0; p < mannequin::kPosePresetCount; ++p)
        m_preset->addItem(tr(mannequin::title(mannequin::PosePreset(p))));
    if (guide)
        m_preset->addItem(tr("The Figure Guide's pose"));
    m_preset->addItem(tr("Your own"));
    (editing ? m_poseStage : m_fitStage)->setChecked(true);
    m_fitBox->setVisible(!editing);
    m_poseBox->setVisible(editing);
    m_sizeBox->setVisible(false);
    m_styleBox->setVisible(false);
    m_style->setCurrentIndex(0);  // none chosen yet (a style only sets the numbers)
    m_joint->setCurrentIndex(int(rig::Joint::LeftElbow));
    m_loading = false;
    setFinish(f);
    // A step already in a style (at the strength last used) shows it.
    for (int s = 0; s < figurestyle::kStyleCount; ++s) {
        const figurestyle::Look look = figurestyle::look(figurestyle::Style(s), f.points, m_strength->value() / 100.0);
        bool same = std::abs(look.blocky - f.blocky) < 0.005;
        for (int k = 0; k < kProportionCount && same; ++k)
            same = std::abs(look.proportions[std::size_t(k)] - f.proportions[std::size_t(k)]) < 0.005;
        if (same && rig::shaped(f)) {
            m_loading = true;
            m_style->setCurrentIndex(s + 1);
            m_loading = false;
            break;
        }
    }
    showStyleOptions();
    setStatus({}, false);
}

void PoseFigurePanel::setFinish(const PosedFinish& f)
{
    m_f = f;
    m_loading = true;
    m_blend->setValue(f.blend);
    m_feetFlat->setChecked(f.feetFlat);
    m_onGround->setChecked(f.onGround);
    for (int i = 0; i < kProportionCount; ++i) {
        const int v = int(std::lround(f.proportions[std::size_t(i)] * 100));
        m_sizeSlider[i]->setValue(v);
        m_sizePercent[i]->setValue(v);
    }
    m_blockySlider->setValue(int(std::lround(f.blocky * 100)));
    m_blocky->setValue(int(std::lround(f.blocky * 100)));
    showSizePreset();
    // Which pose it is, if it's one in the list.
    const auto same = [&](const rig::Angles& a) {
        for (int j = 0; j < kRigJointCount; ++j) {
            if (f.feetFlat && (rig::Joint(j) == rig::Joint::LeftAnkle || rig::Joint(j) == rig::Joint::RightAnkle))
                continue;
            for (int k = 0; k < 2; ++k)
                if (std::abs(a[std::size_t(j)][std::size_t(k)] - f.angles[std::size_t(j)][std::size_t(k)]) > 0.05)
                    return false;
        }
        return true;
    };
    int preset = m_preset->count() - 1;
    if (same(rig::restAngles(f.points)))
        preset = kAsMade;
    else
        for (int p = 0; p < mannequin::kPosePresetCount; ++p)
            if (same(rig::fromMannequin(mannequin::preset(mannequin::PosePreset(p)))))
                preset = p + 1;
    if (m_preset->count() > 0)
        m_preset->setCurrentIndex(preset);
    m_loading = false;
    showJoint();
}

void PoseFigurePanel::showJoint()
{
    const auto joint = rig::Joint(std::max(0, m_joint->currentIndex()));
    const bool flatAnkle =
        m_feetFlat->isChecked() && (joint == rig::Joint::LeftAnkle || joint == rig::Joint::RightAnkle);
    const bool was = m_loading;
    m_loading = true;
    for (int k = 0; k < 2; ++k) {
        const bool used = k < rig::angleCount(joint);
        m_angleLabel[k]->setVisible(used);
        m_angle[k]->setVisible(used);
        if (!used)
            continue;
        m_angleLabel[k]->setText(tr(rig::angleTitle(joint, k)) + QLatin1Char(':'));
        const double value = m_f.angles[std::size_t(joint)][std::size_t(k)];
        const auto range = rig::angleRange(joint, k);
        // Wide enough for the angle it was made at, whatever that is.
        m_angle[k]->setRange(std::min(range[0], std::floor(value)), std::max(range[1], std::ceil(value)));
        m_angle[k]->setValue(value);
        m_angle[k]->setEnabled(!flatAnkle);
        m_angle[k]->setToolTip(flatAnkle ? tr("Feet flat on the ground is ticked: the feet keep their angle")
                                         : QString());
    }
    m_loading = was;
}

bool PoseFigurePanel::posing() const
{
    return !m_fitStage->isChecked();  // posing, or setting the proportions
}

bool PoseFigurePanel::mirror() const
{
    return m_mirror->isChecked();
}

int PoseFigurePanel::joint() const
{
    return m_joint->currentIndex();
}

void PoseFigurePanel::setJoint(int joint)
{
    const bool was = m_loading;
    m_loading = true;
    m_joint->setCurrentIndex(joint);
    m_loading = was;
    showJoint();
}

double PoseFigurePanel::blend() const
{
    return m_blend->value();
}

bool PoseFigurePanel::feetFlat() const
{
    return m_feetFlat->isChecked();
}

bool PoseFigurePanel::onGround() const
{
    return m_onGround->isChecked();
}

void PoseFigurePanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #d04020;") : QString());
    m_status->setVisible(!text.isEmpty());
}

}  // namespace forge::app
