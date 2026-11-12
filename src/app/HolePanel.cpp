// SPDX-License-Identifier: GPL-3.0-or-later
#include "HolePanel.h"

#include "forge/core/SketchShape.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

namespace forge::app {

namespace {

QDoubleSpinBox* lengthBox(double lo, double hi, const QString& tip)
{
    auto* box = new QDoubleSpinBox;
    box->setRange(lo, hi);
    box->setDecimals(2);
    box->setSingleStep(0.5);
    box->setSuffix(QObject::tr(" mm"));
    box->setToolTip(tip);
    box->setMinimumWidth(110);
    return box;
}

}  // namespace

HolePanel::HolePanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    m_title->setWordWrap(true);
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    layout->addWidget(m_title);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    auto* kindGroup = new QGroupBox(tr("Hole"));
    auto* k = new QVBoxLayout(kindGroup);
    k->setSpacing(2);
    // Each list has its label right on top of it.
    k->addWidget(new QLabel(tr("Type:")));
    m_kind = new QComboBox;
    m_kind->addItem(tr("Plain"));
    m_kind->addItem(tr("Counterbored (flat step for a bolt head)"));
    m_kind->addItem(tr("Countersunk (cone for a flat-head screw)"));
    k->addWidget(m_kind);
    k->addSpacing(6);
    k->addWidget(new QLabel(tr("Size:")));
    m_size = new QComboBox;
    m_size->addItem(tr("Custom (type the sizes)"), QString());
    for (const sketch::HoleSize& h : sketch::holeSizes())
        m_size->addItem(QString::fromUtf8(h.name), QString::fromUtf8(h.name));
    m_size->setToolTip(tr("A standard metric screw size fills in the sizes below"));
    k->addWidget(m_size);
    k->addSpacing(6);
    m_fitLabel = new QLabel(tr("Made for:"));
    k->addWidget(m_fitLabel);
    m_fit = new QComboBox;
    m_fit->addItem(tr("A bolt to pass through (clearance)"));
    m_fit->addItem(tr("Cutting a thread (tap drill)"));
    k->addWidget(m_fit);
    k->addSpacing(6);
    k->addWidget(new QLabel(tr("Diameter:")));
    m_diameter = lengthBox(0.1, 1000, tr("How wide the hole is"));
    k->addWidget(m_diameter);
    m_depthLabel = new QLabel(tr("Depth:"));
    k->addWidget(m_depthLabel);
    m_depth = lengthBox(0.1, 10000, tr("How deep the hole goes"));
    k->addWidget(m_depth);
    m_throughAll = new QCheckBox(tr("Through all"));
    m_throughAll->setToolTip(tr("Right through the shape, however thick it gets"));
    k->addWidget(m_throughAll);
    m_headDiameterLabel = new QLabel(tr("Counterbore diameter:"));
    k->addWidget(m_headDiameterLabel);
    m_headDiameter = lengthBox(0.1, 1000, tr("How wide the step for the bolt head is"));
    k->addWidget(m_headDiameter);
    m_headDepthLabel = new QLabel(tr("Counterbore depth:"));
    k->addWidget(m_headDepthLabel);
    m_headDepth = lengthBox(0.1, 1000, tr("How deep the step for the bolt head is"));
    k->addWidget(m_headDepth);
    m_sinkDiameterLabel = new QLabel(tr("Countersink diameter:"));
    k->addWidget(m_sinkDiameterLabel);
    m_sinkDiameter = lengthBox(0.1, 1000, tr("How wide the cone is at the face"));
    k->addWidget(m_sinkDiameter);
    m_sinkAngleLabel = new QLabel(tr("Countersink angle:"));
    k->addWidget(m_sinkAngleLabel);
    m_sinkAngle = new QDoubleSpinBox;
    m_sinkAngle->setRange(10, 170);
    m_sinkAngle->setDecimals(1);
    m_sinkAngle->setSuffix(QStringLiteral("°"));
    m_sinkAngle->setToolTip(tr("The cone's full angle (90° for metric flat-head screws)"));
    k->addWidget(m_sinkAngle);
    layout->addWidget(kindGroup);

    auto* spotGroup = new QGroupBox(tr("Where"));
    auto* sp = new QVBoxLayout(spotGroup);
    auto* spotHelp = new QLabel(tr("Click on the face to add a hole. Type to move one (mm from the face's "
                                   "middle)."));
    spotHelp->setWordWrap(true);
    spotHelp->setStyleSheet(QStringLiteral("color: palette(mid);"));
    sp->addWidget(spotHelp);
    m_spots = new QTableWidget(0, 2);
    m_spots->setHorizontalHeaderLabels({tr("Across (mm)"), tr("Up (mm)")});
    m_spots->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_spots->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_spots->setMinimumHeight(110);
    sp->addWidget(m_spots);
    auto* spotButtons = new QHBoxLayout;
    m_remove = new QPushButton(tr("Remove"));
    m_remove->setToolTip(tr("Remove the hole picked in the list"));
    m_sketch = new QPushButton(tr("Place exactly..."));
    m_sketch->setToolTip(tr("Open the holes' spots in a sketch, to place them with dimensions and rules"));
    spotButtons->addWidget(m_remove);
    spotButtons->addWidget(m_sketch);
    sp->addLayout(spotButtons);
    layout->addWidget(spotGroup);

    auto* buttons = new QHBoxLayout;
    m_apply = new QPushButton(tr("Apply"));
    m_apply->setDefault(true);
    auto* cancel = new QPushButton(tr("Cancel"));
    buttons->addWidget(m_apply);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);
    layout->addStretch(1);

    connect(m_kind, &QComboBox::currentIndexChanged, this, [this] {
        updateLabels();
        emit changed();
    });
    connect(m_size, &QComboBox::currentIndexChanged, this, &HolePanel::sizeChosen);
    connect(m_fit, &QComboBox::currentIndexChanged, this, &HolePanel::sizeChosen);
    for (QDoubleSpinBox* box : {m_diameter, m_headDiameter, m_headDepth, m_sinkDiameter, m_sinkAngle})
        connect(box, &QDoubleSpinBox::valueChanged, this, &HolePanel::typedSize);
    connect(m_depth, &QDoubleSpinBox::valueChanged, this, [this] {
        if (!m_filling)
            emit changed();
    });
    connect(m_throughAll, &QCheckBox::toggled, this, [this] {
        updateLabels();
        emit changed();
    });
    connect(m_spots, &QTableWidget::cellChanged, this, [this](int row, int) {
        if (m_filling)
            return;
        bool okX = false, okY = false;
        const double x = m_spots->item(row, 0) ? m_spots->item(row, 0)->text().toDouble(&okX) : 0;
        const double y = m_spots->item(row, 1) ? m_spots->item(row, 1)->text().toDouble(&okY) : 0;
        if (okX && okY)
            emit spotEdited(row, x, y);
    });
    connect(m_remove, &QPushButton::clicked, this, [this] {
        const int row = m_spots->currentRow();
        if (row >= 0)
            emit removeSpotRequested(row);
    });
    connect(m_sketch, &QPushButton::clicked, this, &HolePanel::sketchRequested);
    connect(m_apply, &QPushButton::clicked, this, &HolePanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &HolePanel::cancelRequested);
}

void HolePanel::start(const QString& title, const HoleSpec& hole, bool throughAll, bool editing)
{
    m_filling = true;
    m_title->setText(title);
    m_kind->setCurrentIndex(int(hole.kind));
    const int sizeIndex = m_size->findData(QString::fromStdString(hole.size));
    m_size->setCurrentIndex(sizeIndex >= 0 ? sizeIndex : 0);
    m_fit->setCurrentIndex(hole.tapped ? 1 : 0);
    m_diameter->setValue(hole.diameter);
    m_depth->setValue(hole.depth);
    m_throughAll->setChecked(throughAll);
    m_headDiameter->setValue(hole.headDiameter);
    m_headDepth->setValue(hole.headDepth);
    m_sinkDiameter->setValue(hole.countersinkDiameter);
    m_sinkAngle->setValue(hole.countersinkAngle);
    m_apply->setToolTip(editing ? tr("Change the holes") : tr("Drill the holes"));
    m_filling = false;
    updateLabels();
}

HoleSpec HolePanel::settings() const
{
    HoleSpec h;
    h.kind = HoleKind(m_kind->currentIndex());
    h.size = m_size->currentData().toString().toStdString();
    h.tapped = m_fit->currentIndex() == 1;
    h.diameter = m_diameter->value();
    h.depth = m_depth->value();
    h.headDiameter = m_headDiameter->value();
    h.headDepth = m_headDepth->value();
    h.countersinkDiameter = m_sinkDiameter->value();
    h.countersinkAngle = m_sinkAngle->value();
    return h;
}

bool HolePanel::throughAll() const
{
    return m_throughAll->isChecked();
}

void HolePanel::setSpots(const std::vector<sketch::Point>& spots, bool placed)
{
    m_filling = true;
    m_spots->setRowCount(int(spots.size()));
    for (std::size_t i = 0; i < spots.size(); ++i) {
        m_spots->setItem(int(i), 0, new QTableWidgetItem(QString::number(spots[i].x, 'f', 2)));
        m_spots->setItem(int(i), 1, new QTableWidgetItem(QString::number(spots[i].y, 'f', 2)));
    }
    m_filling = false;
    m_remove->setEnabled(!spots.empty());
    m_sketch->setEnabled(placed);
}

void HolePanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #c0392b;") : QString());
}

void HolePanel::setApplyEnabled(bool on)
{
    m_apply->setEnabled(on);
}

void HolePanel::sizeChosen()
{
    if (m_filling)
        return;
    const std::string size = m_size->currentData().toString().toStdString();
    if (!size.empty()) {
        const HoleSpec sized = sketch::sizedAs(settings(), size, m_fit->currentIndex() == 1);
        m_filling = true;
        m_diameter->setValue(sized.diameter);
        m_headDiameter->setValue(sized.headDiameter);
        m_headDepth->setValue(sized.headDepth);
        m_sinkDiameter->setValue(sized.countersinkDiameter);
        m_sinkAngle->setValue(sized.countersinkAngle);
        m_filling = false;
    }
    updateLabels();
    emit changed();
}

void HolePanel::typedSize()
{
    if (m_filling)
        return;
    // A size typed by hand is no longer the standard one.
    if (m_size->currentIndex() != 0) {
        const QSignalBlocker block(m_size);
        m_size->setCurrentIndex(0);
    }
    updateLabels();
    emit changed();
}

void HolePanel::updateLabels()
{
    const auto kind = HoleKind(m_kind->currentIndex());
    const bool standard = m_size->currentIndex() != 0;
    m_fitLabel->setVisible(standard);
    m_fit->setVisible(standard);
    for (QWidget* w : {static_cast<QWidget*>(m_headDiameterLabel), static_cast<QWidget*>(m_headDiameter),
                       static_cast<QWidget*>(m_headDepthLabel), static_cast<QWidget*>(m_headDepth)})
        w->setVisible(kind == HoleKind::Counterbored);
    for (QWidget* w : {static_cast<QWidget*>(m_sinkDiameterLabel), static_cast<QWidget*>(m_sinkDiameter),
                       static_cast<QWidget*>(m_sinkAngleLabel), static_cast<QWidget*>(m_sinkAngle)})
        w->setVisible(kind == HoleKind::Countersunk);
    m_depth->setEnabled(!m_throughAll->isChecked());
    m_depthLabel->setEnabled(!m_throughAll->isChecked());
}

}  // namespace forge::app
