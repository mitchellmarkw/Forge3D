// SPDX-License-Identifier: GPL-3.0-or-later
#include "PosePanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include <cmath>

namespace forge::app {

PosePanel::PosePanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel(tr("Pose Test"));
    QFont bold = title->font();
    bold.setBold(true);
    title->setFont(bold);
    layout->addWidget(title);
    auto* help = new QLabel(tr("Turn the joints to see how far they go before parts hit (shown in red). Drag a "
                               "part in the view to turn its joint, or choose a joint and type its angle. The "
                               "parts aren't changed: Done puts them back as built."));
    help->setWordWrap(true);
    help->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(help);

    layout->addWidget(new QLabel(tr("Joints:")));
    m_joints = new QListWidget;
    m_joints->setToolTip(tr("The joints made with Quick Hinge and Quick Socket. Click one, or click a part in the "
                            "view to choose the joint it turns on."));
    m_joints->setMaximumHeight(150);
    layout->addWidget(m_joints);

    m_angleBox = new QWidget;
    auto* angles = new QGridLayout(m_angleBox);
    angles->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < 3; ++i) {
        m_angleLabels[i] = new QLabel;
        m_angle[i] = new QDoubleSpinBox;
        m_angle[i]->setRange(-180.0, 180.0);
        m_angle[i]->setDecimals(1);
        m_angle[i]->setSingleStep(5.0);
        m_angle[i]->setSuffix(QStringLiteral("°"));
        m_angle[i]->setWrapping(true);
        angles->addWidget(m_angleLabels[i], i, 0);
        angles->addWidget(m_angle[i], i, 1);
        connect(m_angle[i], &QDoubleSpinBox::valueChanged, this, [this, i](double v) {
            if (i == 0) {
                const QSignalBlocker block(m_slider);
                m_slider->setValue(int(std::lround(v)));
            }
            emit anglesEdited(false);
        });
    }
    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setRange(-180, 180);
    m_slider->setToolTip(tr("Drag to turn the joint"));
    angles->addWidget(m_slider, 3, 0, 1, 2);
    connect(m_slider, &QSlider::valueChanged, this, [this](int v) {
        {
            const QSignalBlocker block(m_angle[0]);
            m_angle[0]->setValue(v);
        }
        emit anglesEdited(m_slider->isSliderDown());
    });
    connect(m_slider, &QSlider::sliderReleased, this, [this] { emit anglesEdited(false); });
    layout->addWidget(m_angleBox);

    auto* jointButtons = new QHBoxLayout;
    m_range = new QPushButton(tr("Find Range"));
    m_range->setToolTip(tr("Turn the joint each way from straight until parts hit, and say how far it went"));
    auto* straighten = new QPushButton(tr("Straighten"));
    straighten->setToolTip(tr("Put this joint back to straight"));
    jointButtons->addWidget(m_range);
    jointButtons->addWidget(straighten);
    layout->addLayout(jointButtons);
    m_rangeText = new QLabel;
    m_rangeText->setWordWrap(true);
    layout->addWidget(m_rangeText);

    m_hits = new QLabel;
    m_hits->setWordWrap(true);
    layout->addWidget(m_hits);

    // Will it stand? The centre of weight against where the feet touch, as posed.
    auto* stand = new QGroupBox(tr("Will it stand?"));
    auto* standLayout = new QVBoxLayout(stand);
    m_showBalance = new QCheckBox(tr("Show the balance"));
    m_showBalance->setToolTip(tr("The centre of weight (gold, with a line down to the ground) and where it touches "
                                 "the ground: green if it stands, red if it tips, as you pose it"));
    standLayout->addWidget(m_showBalance);
    m_balance = new QLabel;
    m_balance->setWordWrap(true);
    standLayout->addWidget(m_balance);
    m_standBox = new QWidget;
    auto* standPeg = new QGridLayout(m_standBox);
    standPeg->setContentsMargins(0, 0, 0, 0);
    standPeg->addWidget(new QLabel(tr("Stand peg:")), 0, 0);
    m_standPeg = new QDoubleSpinBox;
    m_standPeg->setRange(0.5, 20.0);
    m_standPeg->setDecimals(2);
    m_standPeg->setSingleStep(0.1);
    m_standPeg->setPrefix(tr("Ø "));
    m_standPeg->setSuffix(tr(" mm"));
    m_standPeg->setValue(3.0);
    m_standPeg->setToolTip(tr("The diameter of the peg on the stand"));
    standPeg->addWidget(m_standPeg, 0, 1);
    standPeg->addWidget(new QLabel(tr("Hole depth:")), 1, 0);
    m_standDepth = new QDoubleSpinBox;
    m_standDepth->setRange(0.5, 40.0);
    m_standDepth->setDecimals(1);
    m_standDepth->setSingleStep(0.5);
    m_standDepth->setSuffix(tr(" mm"));
    m_standDepth->setValue(4.0);
    standPeg->addWidget(m_standDepth, 1, 1);
    auto* pegs = new QPushButton(tr("Peg Hole in Each Foot"));
    pegs->setToolTip(tr("A hole for a stand's peg, straight up into the middle of each sole that touches the ground "
                        "(Quick Socket steps, with your printer's fit)"));
    standPeg->addWidget(pegs, 2, 0, 1, 2);
    auto* base = new QPushButton(tr("Make a Display Base..."));
    base->setToolTip(tr("A round, oval, square or hexagonal base for the figure as it's posed now, with a peg "
                        "standing up right under each foot (or holes, if the feet have pegs)"));
    standLayout->addWidget(m_standBox);
    standLayout->addWidget(base);
    auto* picture = new QPushButton(tr("Picture of This Pose..."));
    picture->setToolTip(tr("A clean picture (PNG) or a turntable (GIF) of the figure as it's posed now, without "
                           "keeping the pose"));
    standLayout->addWidget(picture);
    layout->addWidget(stand);
    connect(m_showBalance, &QCheckBox::toggled, this, [this](bool on) {
        m_balance->setVisible(on);
        emit balanceToggled(on);
    });
    connect(pegs, &QPushButton::clicked, this, &PosePanel::standPegsRequested);
    connect(base, &QPushButton::clicked, this, &PosePanel::displayBaseRequested);
    connect(picture, &QPushButton::clicked, this, &PosePanel::pictureRequested);
    m_balance->hide();

    // Make Room: cut away just enough for the joint to turn to an angle.
    auto* room = new QGroupBox(tr("Make room to turn further"));
    m_roomBox = room;
    auto* roomLayout = new QVBoxLayout(room);
    auto* roomHelp = new QLabel(tr("Trims away just what's in the way, so this joint turns as far as you want (a "
                                   "90° knee). It turns from where it is now."));
    roomHelp->setWordWrap(true);
    roomLayout->addWidget(roomHelp);
    const auto labelled = [](const QString& text, QComboBox* box) {
        auto* holder = new QWidget;
        auto* column = new QVBoxLayout(holder);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(0);
        column->addWidget(new QLabel(text));
        column->addWidget(box);
        return holder;
    };
    m_roomWhich = new QComboBox;
    m_roomWhich->addItem(tr("Bend"));
    m_roomWhich->addItem(tr("Bend sideways"));
    m_roomWhich->addItem(tr("Twist"));
    m_roomWhichRow = labelled(tr("Which way:"), m_roomWhich);
    roomLayout->addWidget(m_roomWhichRow);
    auto* roomGrid = new QGridLayout;
    roomGrid->addWidget(new QLabel(tr("Turn to:")), 0, 0);
    m_roomTarget = new QDoubleSpinBox;
    m_roomTarget->setRange(-180.0, 180.0);
    m_roomTarget->setDecimals(0);
    m_roomTarget->setSingleStep(5.0);
    m_roomTarget->setSuffix(tr("°"));
    m_roomTarget->setValue(90.0);
    m_roomTarget->setToolTip(tr("The angle the joint should reach (from straight; − turns the other way)"));
    roomGrid->addWidget(m_roomTarget, 0, 1);
    roomGrid->addWidget(new QLabel(tr("Clearance:")), 1, 0);
    m_roomClearance = new QDoubleSpinBox;
    m_roomClearance->setRange(0.0, 2.0);
    m_roomClearance->setDecimals(2);
    m_roomClearance->setSingleStep(0.05);
    m_roomClearance->setSuffix(tr(" mm"));
    m_roomClearance->setValue(0.3);
    m_roomClearance->setToolTip(tr("Room to spare where the parts meet at that angle"));
    roomGrid->addWidget(m_roomClearance, 1, 1);
    roomLayout->addLayout(roomGrid);
    m_roomFrom = new QComboBox;
    m_roomFrom->addItem(tr("The part it hits (it stays still)"));
    m_roomFrom->addItem(tr("The part that turns"));
    m_roomFrom->setToolTip(tr("Which part loses the material in the way"));
    roomLayout->addWidget(labelled(tr("Cut it from:"), m_roomFrom));
    m_roomButton = new QPushButton(tr("Make Room"));
    m_roomButton->setToolTip(tr("Cuts the parts (one undo step); the cut shows in each part's history"));
    roomLayout->addWidget(m_roomButton);
    layout->addWidget(room);
    connect(m_roomButton, &QPushButton::clicked, this, &PosePanel::makeRoomRequested);

    auto* holdRow = new QHBoxLayout;
    m_hold = new QLabel;
    m_hold->setWordWrap(true);
    auto* hold = new QPushButton(tr("Hold Still"));
    hold->setToolTip(tr("Hold the selected part still and turn the others from it (click the part first)"));
    holdRow->addWidget(m_hold, 1);
    holdRow->addWidget(hold);
    layout->addLayout(holdRow);
    auto* buildPose = new QPushButton(tr("Back to the Build Pose"));
    buildPose->setToolTip(tr("Every joint straight, as the parts are made"));
    layout->addWidget(buildPose);

    auto* saved = new QGroupBox(tr("Saved poses"));
    auto* savedColumn = new QVBoxLayout(saved);
    auto* posesBox = new QWidget;
    auto* posesColumn = new QVBoxLayout(posesBox);
    posesColumn->setContentsMargins(0, 0, 0, 0);
    posesColumn->setSpacing(0);
    posesColumn->addWidget(new QLabel(tr("Pose:")));
    m_poses = new QComboBox;
    m_poses->setToolTip(tr("Choose a saved pose to show it"));
    posesColumn->addWidget(m_poses);
    savedColumn->addWidget(posesBox);
    auto* poseButtons = new QHBoxLayout;
    auto* save = new QPushButton(tr("Save Pose..."));
    save->setToolTip(tr("Keep this pose by name, in the file (the parts stay as built)"));
    m_deletePose = new QPushButton(tr("Delete"));
    m_deletePose->setToolTip(tr("Forget the chosen pose"));
    poseButtons->addWidget(save);
    poseButtons->addWidget(m_deletePose);
    savedColumn->addLayout(poseButtons);
    layout->addWidget(saved);

    m_status = new QLabel;
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    auto* done = new QPushButton(tr("Done"));
    done->setDefault(true);
    done->setToolTip(tr("Close Pose Test; the parts show as built again"));
    layout->addWidget(done);
    layout->addStretch(1);

    connect(m_joints, &QListWidget::currentRowChanged, this, &PosePanel::jointChosen);
    connect(m_range, &QPushButton::clicked, this, &PosePanel::rangeRequested);
    connect(straighten, &QPushButton::clicked, this, &PosePanel::straightenRequested);
    connect(buildPose, &QPushButton::clicked, this, &PosePanel::buildPoseRequested);
    connect(hold, &QPushButton::clicked, this, &PosePanel::holdRequested);
    connect(m_poses, &QComboBox::activated, this, &PosePanel::poseChosen);
    connect(save, &QPushButton::clicked, this, &PosePanel::savePoseRequested);
    connect(m_deletePose, &QPushButton::clicked, this, &PosePanel::deletePoseRequested);
    connect(done, &QPushButton::clicked, this, &PosePanel::doneRequested);
    showJoint(-1, pose::JointKind::Hinge, {0, 0, 0});
}

void PosePanel::setJoints(const QStringList& names)
{
    const QSignalBlocker block(m_joints);
    m_joints->clear();
    m_joints->addItems(names);
}

int PosePanel::joint() const { return m_joints->currentRow(); }

void PosePanel::showJoint(int index, pose::JointKind kind, const std::array<double, 3>& a)
{
    m_kind = kind;
    {
        const QSignalBlocker block(m_joints);
        m_joints->setCurrentRow(index);
    }
    const bool any = index >= 0;
    const bool ball = kind == pose::JointKind::Ball;
    const bool turns = kind != pose::JointKind::Fixed;
    m_angleBox->setVisible(any && turns);
    m_range->setEnabled(any && (kind == pose::JointKind::Hinge || kind == pose::JointKind::Swivel));
    m_roomBox->setVisible(any && turns);
    m_roomWhichRow->setVisible(ball);
    if (!ball)
        m_roomWhich->setCurrentIndex(0);
    m_range->setToolTip(ball ? tr("A ball turns every way: turn it by hand to check it")
                             : tr("Turn the joint each way from straight until parts hit, and say how far it went"));
    m_angleLabels[0]->setText(ball ? tr("Bend:") : kind == pose::JointKind::Swivel ? tr("Turn:") : tr("Bend:"));
    m_angleLabels[1]->setText(tr("Bend sideways:"));
    m_angleLabels[2]->setText(tr("Twist:"));
    for (int i = 1; i < 3; ++i) {
        m_angleLabels[i]->setVisible(ball);
        m_angle[i]->setVisible(ball);
    }
    for (int i = 0; i < 3; ++i) {
        const QSignalBlocker block(m_angle[i]);
        m_angle[i]->setValue(a[std::size_t(i)]);
    }
    const QSignalBlocker block(m_slider);
    m_slider->setValue(int(std::lround(a[0])));
    if (any && kind == pose::JointKind::Fixed)
        setStatus(tr("This joint holds its parts together without turning (a peg with flats)."), false);
}

std::array<double, 3> PosePanel::angles() const
{
    return {m_angle[0]->value(), m_angle[1]->value(), m_angle[2]->value()};
}

void PosePanel::setHold(const QString& name)
{
    m_hold->setText(name.isEmpty() ? tr("<i>No joints to hold from.</i>")
                                   : tr("Held still: <b>%1</b>").arg(name.toHtmlEscaped()));
}

void PosePanel::setHits(const QString& text, bool problem)
{
    m_hits->setText(text);
    m_hits->setStyleSheet(problem ? QStringLiteral("color: #c0392b; font-weight: bold;") : QString());
}

void PosePanel::setRange(const QString& text)
{
    m_rangeText->setText(text);
}

void PosePanel::setPoses(const QStringList& names, int current)
{
    const QSignalBlocker block(m_poses);
    m_poses->clear();
    m_poses->addItems(names);
    m_poses->setCurrentIndex(current);
    m_deletePose->setEnabled(!names.isEmpty());
}

void PosePanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #c0392b;") : QString());
}

double PosePanel::roomTarget() const
{
    return m_roomTarget->value();
}

int PosePanel::roomWhich() const
{
    return m_kind == pose::JointKind::Ball ? m_roomWhich->currentIndex() : 0;
}

bool PosePanel::roomFromTurning() const
{
    return m_roomFrom->currentIndex() == 1;
}

double PosePanel::roomClearance() const
{
    return m_roomClearance->value();
}

void PosePanel::setRoomBusy(bool busy)
{
    m_roomButton->setEnabled(!busy);
    m_roomButton->setText(busy ? tr("Making Room...") : tr("Make Room"));
}

bool PosePanel::showBalance() const
{
    return m_showBalance->isChecked();
}

void PosePanel::setShowBalance(bool on)
{
    m_showBalance->setChecked(on);
}

void PosePanel::setBalance(const QString& text, bool problem)
{
    m_balance->setText(text);
    m_balance->setStyleSheet(problem ? QStringLiteral("color: #d04020;") : QString());
}

double PosePanel::standPegSize() const
{
    return m_standPeg->value();
}

double PosePanel::standPegDepth() const
{
    return m_standDepth->value();
}

}  // namespace forge::app
