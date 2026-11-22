// SPDX-License-Identifier: GPL-3.0-or-later
#include "HandPanel.h"

#include "Standards.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QVBoxLayout>

#include <cmath>

namespace forge::app {
namespace {

QSettings preferences()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"), QStringLiteral("Forge3D"));
}

QDoubleSpinBox* spin(double low, double high, double step, const QString& suffix, int decimals = 2)
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

HandPanel::HandPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel(tr("Make Hand"));
    QFont bold = title->font();
    bold.setBold(true);
    title->setFont(bold);
    layout->addWidget(title);
    auto* intro = new QLabel(tr("A hand at your figure's scale, standing on its wrist, with the socket for the wrist "
                                "peg cut in. Sculpt it further afterwards if you like."));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* handBox = new QGroupBox(tr("Hand"));
    auto* hand = new QVBoxLayout(handBox);
    m_pose = new QComboBox;
    m_pose->addItem(tr("Fist"));
    m_pose->addItem(tr("Gripping (a hole for a handle)"));
    m_pose->addItem(tr("Open"));
    m_pose->addItem(tr("Relaxed"));
    m_pose->addItem(tr("Pointing"));
    hand->addWidget(labelled(tr("Pose:"), m_pose));
    m_which = new QComboBox;
    m_which->addItem(tr("Right"));
    m_which->addItem(tr("Left"));
    m_which->addItem(tr("A pair (right and left)"));
    hand->addWidget(labelled(tr("Which hand:"), m_which));
    m_scale = new QComboBox;
    m_scale->addItem(tr("1:6 (12-inch figures)"));
    m_scale->addItem(tr("1:10 (7-inch figures)"));
    m_scale->addItem(tr("1:12 (6-inch figures)"));
    m_scale->addItem(tr("1:18 (3.75-inch figures)"));
    m_scale->addItem(tr("Set the length"));
    m_scale->setToolTip(tr("Sets the hand's length from a person's (189 mm) at the figure's scale"));
    hand->addWidget(labelled(tr("Scale:"), m_scale));
    auto* handForm = new QFormLayout;
    m_length = spin(5.0, 300.0, 0.5, tr(" mm"));
    m_length->setToolTip(tr("From the wrist to the tip of the middle finger, held straight"));
    handForm->addRow(tr("Hand length:"), m_length);
    m_fullness = spin(80.0, 200.0, 5.0, tr("%"), 0);
    m_fullness->setToolTip(tr("Palm thickness and finger girth: 100% is a person's; more is chunkier and sturdier, "
                              "as toy hands are"));
    handForm->addRow(tr("Fullness:"), m_fullness);
    hand->addLayout(handForm);
    layout->addWidget(handBox);

    auto* gripBox = new QGroupBox(tr("Handle"));
    m_gripBox = gripBox;
    auto* gripForm = new QFormLayout(gripBox);
    m_handle = spin(0.3, 100.0, 0.1, tr(" mm"));
    m_handle->setToolTip(tr("The diameter of the handles the hand holds: the fingers wrap round it"));
    gripForm->addRow(tr("Handle diameter:"), m_handle);
    m_gripFit = spin(0.0, 1.0, 0.05, tr(" mm"));
    m_gripFit->setToolTip(tr("Room all round the handle: less grips tighter"));
    gripForm->addRow(tr("Fit:"), m_gripFit);
    m_angle = spin(-45.0, 45.0, 5.0, tr("°"), 0);
    m_angle->setToolTip(tr("How far the handle tips down towards the little finger"));
    gripForm->addRow(tr("Angle:"), m_angle);
    layout->addWidget(gripBox);

    auto* wristBox = new QGroupBox(tr("Wrist socket"));
    auto* wrist = new QVBoxLayout(wristBox);
    m_standard = new StandardRow;
    wrist->addWidget(m_standard);
    m_wristKind = new QComboBox;
    m_wristKind->addItem(tr("For a peg"));
    m_wristKind->addItem(tr("For a ball (snap-in)"));
    m_wristKind->addItem(tr("None"));
    m_wristKind->setToolTip(tr("The socket the arm's wrist peg (Hand Ports) plugs into; the wrist grows to hold it"));
    wrist->addWidget(labelled(tr("Socket:"), m_wristKind));
    m_wristSizes = new QWidget;
    auto* sizes = new QVBoxLayout(m_wristSizes);
    sizes->setContentsMargins(0, 0, 0, 0);
    m_shape = new QComboBox;
    m_shape->addItem(tr("Round"));
    m_shape->addItem(tr("Square"));
    m_shape->addItem(tr("Hexagon"));
    m_shape->addItem(tr("Octagon"));
    m_shapeRow = labelled(tr("Peg shape:"), m_shape);
    sizes->addWidget(m_shapeRow);
    auto* sizeForm = new QFormLayout;
    m_size = spin(0.5, 50.0, 0.1, tr(" mm"));
    m_sizeLabel = new QLabel(tr("Peg diameter:"));
    sizeForm->addRow(m_sizeLabel, m_size);
    m_depth = spin(0.5, 100.0, 0.5, tr(" mm"));
    m_depth->setToolTip(tr("How far the peg goes in"));
    m_depthRow = new QLabel(tr("Peg length:"));
    sizeForm->addRow(m_depthRow, m_depth);
    sizes->addLayout(sizeForm);
    m_printer = new QComboBox;
    m_printer->addItem(tr("Resin"));
    m_printer->addItem(tr("FDM (filament)"));
    sizes->addWidget(labelled(tr("Printed on:"), m_printer));
    auto* fitForm = new QFormLayout;
    m_fit = spin(0.0, 1.0, 0.05, tr(" mm"));
    m_fit->setToolTip(tr("Room all round the peg or ball"));
    fitForm->addRow(tr("Fit:"), m_fit);
    sizes->addLayout(fitForm);
    wrist->addWidget(m_wristSizes);
    layout->addWidget(wristBox);

    m_status = new QLabel;
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    auto* buttons = new QHBoxLayout;
    auto* cancel = new QPushButton(tr("Cancel"));
    m_apply = new QPushButton(tr("Make"));
    m_apply->setDefault(true);
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(m_apply);
    layout->addLayout(buttons);
    layout->addStretch(1);

    // Start from what was used last; the wrist sizes from Hand Ports' peg, so they match.
    const QSettings p = preferences();
    m_pose->setCurrentIndex(p.value(QStringLiteral("hand/pose"), 0).toInt());
    m_which->setCurrentIndex(p.value(QStringLiteral("hand/which"), 2).toInt());
    m_scale->setCurrentIndex(p.value(QStringLiteral("hand/scale"), 2).toInt());
    m_length->setValue(p.value(QStringLiteral("hand/length"), hand::lengthAtScale(12.0)).toDouble());
    m_fullness->setValue(p.value(QStringLiteral("hand/fullness"), 120.0).toDouble());
    m_handle->setValue(p.value(QStringLiteral("ports/handle"), 3.0).toDouble());
    m_gripFit->setValue(p.value(QStringLiteral("ports/gripFit"), 0.1).toDouble());
    m_angle->setValue(p.value(QStringLiteral("hand/angle"), 10.0).toDouble());
    const int end = p.value(QStringLiteral("ports/end"), 1).toInt();
    m_wristKind->setCurrentIndex(end == 2 ? 1 : 0);
    m_shape->setCurrentIndex(p.value(QStringLiteral("ports/shape"), 0).toInt());
    m_size->setValue(end == 2 ? p.value(QStringLiteral("ports/ball"), 4.0).toDouble()
                              : p.value(QStringLiteral("ports/size"), 3.0).toDouble());
    m_depth->setValue(p.value(QStringLiteral("ports/length"), 4.0).toDouble());
    m_printer->setCurrentIndex(p.value(QStringLiteral("ports/resin"), true).toBool() ? 0 : 1);
    m_fit->setValue(p.value(QStringLiteral("ports/socketFit"), 0.1).toDouble());

    for (QDoubleSpinBox* s : {m_fullness, m_handle, m_gripFit, m_angle, m_size, m_depth, m_fit})
        connect(s, &QDoubleSpinBox::valueChanged, this, [this] {
            remember();
            emit changed();
        });
    connect(m_length, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        // A length of its own: no longer a scale's.
        bool atScale = false;
        for (int i = 0; i < 4; ++i)
            if (m_scale->currentIndex() == i && std::abs(hand::lengthAtScale(kScales[i]) - v) < 0.01)
                atScale = true;
        if (!atScale) {
            const QSignalBlocker block(m_scale);
            m_scale->setCurrentIndex(4);
        }
        remember();
        emit changed();
    });
    connect(m_scale, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (i >= 0 && i < 4)
            m_length->setValue(std::round(hand::lengthAtScale(kScales[i]) * 100.0) / 100.0);
        remember();
    });
    for (QComboBox* c : {m_pose, m_which, m_wristKind, m_shape, m_printer})
        connect(c, &QComboBox::currentIndexChanged, this, [this] {
            refresh();
            remember();
            emit changed();
        });
    connect(m_printer, &QComboBox::currentIndexChanged, this,
            [this] { m_fit->setValue(m_printer->currentIndex() == 0 ? 0.1 : 0.2); });
    connect(m_apply, &QPushButton::clicked, this, &HandPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &HandPanel::cancelRequested);
    refresh();
}

void HandPanel::remember() const
{
    QSettings p = preferences();
    p.setValue(QStringLiteral("hand/pose"), m_pose->currentIndex());
    p.setValue(QStringLiteral("hand/which"), m_which->currentIndex());
    p.setValue(QStringLiteral("hand/scale"), m_scale->currentIndex());
    p.setValue(QStringLiteral("hand/length"), m_length->value());
    p.setValue(QStringLiteral("hand/fullness"), m_fullness->value());
    p.setValue(QStringLiteral("hand/angle"), m_angle->value());
}

void HandPanel::refresh()
{
    m_gripBox->setVisible(m_pose->currentIndex() == 1);
    const int kind = m_wristKind->currentIndex();
    m_wristSizes->setVisible(kind != 2);
    m_shapeRow->setVisible(kind == 0);
    m_depthRow->setVisible(kind == 0);
    m_depth->setVisible(kind == 0);
    m_sizeLabel->setText(kind == 1                       ? tr("Ball diameter:")
                         : m_shape->currentIndex() == 0 ? tr("Peg diameter:")
                                                        : tr("Peg across flats:"));
    fitHeight();
}

void HandPanel::start()
{
    setStatus({}, false);
    refresh();
}

HandSettings HandPanel::settings() const
{
    HandSettings s;
    s.pose = HandPose(m_pose->currentIndex());
    s.left = m_which->currentIndex() == 1;
    s.length = m_length->value();
    s.fullness = m_fullness->value() / 100.0;
    s.handle = m_handle->value();
    s.fit = m_gripFit->value();
    s.gripAngle = m_angle->value();
    return s;
}

bool HandPanel::pair() const
{
    return m_which->currentIndex() == 2;
}

std::optional<SocketFinish> HandPanel::wrist() const
{
    const int kind = m_wristKind->currentIndex();
    if (kind == 2)
        return std::nullopt;
    SocketFinish s;
    s.resin = m_printer->currentIndex() == 0;
    s.fit = m_fit->value();
    if (kind == 1) {
        s.kind = SocketKind::Ball;
        s.size = m_size->value();
        s.snap = true;
        return s;
    }
    s.kind = SocketKind::Peg;
    s.shape = PegShape(m_shape->currentIndex());
    s.size = m_size->value();
    s.depth = m_depth->value();
    return s;
}

void HandPanel::setWrist(const SocketFinish& joint)
{
    {
        const QSignalBlocker a(m_wristKind), b(m_shape), c(m_size), d(m_depth);
        if (joint.kind == SocketKind::Ball) {
            m_wristKind->setCurrentIndex(1);
            m_size->setValue(joint.size);
        } else {
            m_wristKind->setCurrentIndex(0);
            m_shape->setCurrentIndex(int(joint.shape));
            m_size->setValue(joint.size);
            m_depth->setValue(joint.depth);
        }
    }
    refresh();
    emit changed();
}

void HandPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #d04020;") : QString());
}

void HandPanel::setApplyEnabled(bool on)
{
    m_apply->setEnabled(on);
}

void HandPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    fitHeight();
}

void HandPanel::fitHeight()
{
    layout()->activate();
    const int needed = layout()->hasHeightForWidth() ? layout()->heightForWidth(width()) : sizeHint().height();
    if (needed > 0 && minimumHeight() != needed)
        setMinimumHeight(needed);
}

}  // namespace forge::app
