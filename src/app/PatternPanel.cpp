// SPDX-License-Identifier: GPL-3.0-or-later
#include "PatternPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <cmath>

namespace forge::app {

namespace {

QDoubleSpinBox* mmBox(double lo, double hi, const QString& tip)
{
    auto* box = new QDoubleSpinBox;
    box->setRange(lo, hi);
    box->setDecimals(2);
    box->setSingleStep(1.0);
    box->setSuffix(QObject::tr(" mm"));
    box->setToolTip(tip);
    box->setMinimumWidth(110);
    return box;
}

QSpinBox* countBox(const QString& tip)
{
    auto* box = new QSpinBox;
    box->setRange(1, 1000);
    box->setToolTip(tip);
    return box;
}

QComboBox* directionBox()
{
    auto* box = new QComboBox;
    box->addItem(QObject::tr("Across (X)"));
    box->addItem(QObject::tr("Front to back (Y)"));
    box->addItem(QObject::tr("Up (Z)"));
    return box;
}

std::array<double, 3> axisVector(int index, bool reverse)
{
    std::array<double, 3> v{0, 0, 0};
    v[std::size_t(index)] = reverse ? -1.0 : 1.0;
    return v;
}

// The axis a direction is closest to, and whether it points the other way.
std::pair<int, bool> nearestAxis(const std::array<double, 3>& d)
{
    int best = 0;
    for (int k = 1; k < 3; ++k)
        if (std::abs(d[std::size_t(k)]) > std::abs(d[std::size_t(best)]))
            best = k;
    return {best, d[std::size_t(best)] < 0};
}

// A label right on top of its list.
void labelled(QVBoxLayout* layout, const QString& text, QWidget* w)
{
    layout->addWidget(new QLabel(text));
    layout->addWidget(w);
}

}  // namespace

PatternPanel::PatternPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    m_title->setWordWrap(true);
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    layout->addWidget(m_title);
    auto* help = new QLabel(tr("Repeats the shape, or the hole / pocket / groove its last step made, in a row, a "
                               "grid or round a circle. The copies stay linked: change the original and they "
                               "follow."));
    help->setWordWrap(true);
    help->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(help);
    layout->addSpacing(4);
    m_what = new QComboBox;
    labelled(layout, tr("Repeat:"), m_what);
    m_kind = new QComboBox;
    m_kind->addItem(tr("In a row"));
    m_kind->addItem(tr("In a grid"));
    m_kind->addItem(tr("Round a circle"));
    labelled(layout, tr("Pattern:"), m_kind);
    layout->addSpacing(4);

    m_line = new QWidget;
    auto* line = new QVBoxLayout(m_line);
    line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(2);
    m_dir1 = directionBox();
    labelled(line, tr("Direction:"), m_dir1);
    m_reverse1 = new QCheckBox(tr("The other way"));
    line->addWidget(m_reverse1);
    auto* form1 = new QFormLayout;
    m_count1 = countBox(tr("How many, the original included"));
    m_spacing1 = mmBox(0.01, 10000, tr("From one copy to the next"));
    form1->addRow(tr("How many:"), m_count1);
    form1->addRow(tr("Spacing:"), m_spacing1);
    line->addLayout(form1);
    layout->addWidget(m_line);

    m_second = new QWidget;
    auto* second = new QVBoxLayout(m_second);
    second->setContentsMargins(0, 6, 0, 0);
    second->setSpacing(2);
    m_dir2 = directionBox();
    labelled(second, tr("Second direction:"), m_dir2);
    m_reverse2 = new QCheckBox(tr("The other way"));
    second->addWidget(m_reverse2);
    auto* form2 = new QFormLayout;
    m_count2 = countBox(tr("How many rows"));
    m_spacing2 = mmBox(0.01, 10000, tr("From one row to the next"));
    form2->addRow(tr("How many:"), m_count2);
    form2->addRow(tr("Spacing:"), m_spacing2);
    second->addLayout(form2);
    layout->addWidget(m_second);

    m_circle = new QWidget;
    auto* circle = new QVBoxLayout(m_circle);
    circle->setContentsMargins(0, 0, 0, 0);
    circle->setSpacing(2);
    m_axis = new QComboBox;
    m_axis->addItem(tr("A line across (X)"));
    m_axis->addItem(tr("A line front to back (Y)"));
    m_axis->addItem(tr("A line straight up (Z)"));
    labelled(circle, tr("Turning round:"), m_axis);
    auto* form3 = new QFormLayout;
    const char* names[] = {"Centre X:", "Centre Y:", "Centre Z:"};
    for (int k = 0; k < 3; ++k) {
        m_centre[k] = mmBox(-100000, 100000, tr("Where the line it turns round goes through"));
        form3->addRow(tr(names[k]), m_centre[k]);
    }
    m_count = countBox(tr("How many, the original included"));
    m_angle = new QDoubleSpinBox;
    m_angle->setRange(0.1, 360.0);
    m_angle->setDecimals(1);
    m_angle->setSingleStep(15.0);
    m_angle->setSuffix(QStringLiteral("°"));
    m_angle->setToolTip(tr("360°: evenly all the way round; less: spread from the original over this angle"));
    form3->addRow(tr("How many:"), m_count);
    form3->addRow(tr("Angle:"), m_angle);
    circle->addLayout(form3);
    layout->addWidget(m_circle);

    m_status = new QLabel;
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    auto* buttons = new QHBoxLayout;
    m_apply = new QPushButton(tr("Apply"));
    m_apply->setDefault(true);
    auto* cancel = new QPushButton(tr("Cancel"));
    buttons->addWidget(m_apply);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);
    layout->addStretch(1);

    const auto changedNow = [this] {
        if (!m_filling) {
            updateLabels();
            emit changed();
        }
    };
    connect(m_what, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_filling)
            emit repeatChanged(m_what->currentIndex() == 1);
    });
    for (QComboBox* c : {m_kind, m_dir1, m_dir2, m_axis})
        connect(c, &QComboBox::currentIndexChanged, this, changedNow);
    for (QCheckBox* c : {m_reverse1, m_reverse2})
        connect(c, &QCheckBox::toggled, this, changedNow);
    for (QSpinBox* c : {m_count1, m_count2, m_count})
        connect(c, &QSpinBox::valueChanged, this, changedNow);
    for (QDoubleSpinBox* c : {m_spacing1, m_spacing2, m_angle, m_centre[0], m_centre[1], m_centre[2]})
        connect(c, &QDoubleSpinBox::valueChanged, this, changedNow);
    connect(m_apply, &QPushButton::clicked, this, &PatternPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &PatternPanel::cancelRequested);
}

void PatternPanel::start(const QString& title, const std::optional<QString>& feature, const PatternFinish& finish,
                         bool editing)
{
    m_filling = true;
    m_title->setText(title);
    m_what->clear();
    m_what->addItem(tr("The whole shape"));
    m_what->addItem(feature ? tr("Its last step: %1").arg(*feature) : tr("Its last step (nothing to repeat)"));
    if (auto* model = qobject_cast<QStandardItemModel*>(m_what->model()))
        model->item(1)->setEnabled(feature.has_value());
    m_what->setCurrentIndex(finish.feature && feature ? 1 : 0);
    m_what->setEnabled(!editing);  // a pattern made stays what it repeats
    m_kind->setCurrentIndex(int(finish.kind));
    m_filling = false;
    setNumbers(finish);
    m_apply->setToolTip(editing ? tr("Change the pattern") : tr("Make the copies"));
}

void PatternPanel::setNumbers(const PatternFinish& f)
{
    m_filling = true;
    const auto [a1, r1] = nearestAxis(f.dir1);
    const auto [a2, r2] = nearestAxis(f.dir2);
    m_dir1->setCurrentIndex(a1);
    m_reverse1->setChecked(r1);
    m_dir2->setCurrentIndex(a2);
    m_reverse2->setChecked(r2);
    m_count1->setValue(f.count1);
    m_count2->setValue(f.count2);
    m_spacing1->setValue(f.spacing1);
    m_spacing2->setValue(f.spacing2);
    m_axis->setCurrentIndex(nearestAxis(f.axis).first);
    for (int k = 0; k < 3; ++k)
        m_centre[k]->setValue(f.centre[std::size_t(k)]);
    m_count->setValue(f.count);
    m_angle->setValue(f.angle);
    m_filling = false;
    updateLabels();
}

PatternFinish PatternPanel::finish() const
{
    PatternFinish f;
    f.kind = PatternKind(m_kind->currentIndex());
    f.feature = m_what->currentIndex() == 1;
    f.dir1 = axisVector(m_dir1->currentIndex(), m_reverse1->isChecked());
    f.dir2 = axisVector(m_dir2->currentIndex(), m_reverse2->isChecked());
    f.count1 = m_count1->value();
    f.count2 = m_count2->value();
    f.spacing1 = m_spacing1->value();
    f.spacing2 = m_spacing2->value();
    f.axis = axisVector(m_axis->currentIndex(), false);
    for (int k = 0; k < 3; ++k)
        f.centre[std::size_t(k)] = m_centre[k]->value();
    f.count = m_count->value();
    f.angle = m_angle->value();
    return f;
}

void PatternPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #c0392b;") : QString());
}

void PatternPanel::setApplyEnabled(bool on)
{
    m_apply->setEnabled(on);
}

void PatternPanel::updateLabels()
{
    const auto kind = PatternKind(m_kind->currentIndex());
    m_line->setVisible(kind != PatternKind::Circle);
    m_second->setVisible(kind == PatternKind::Grid);
    m_circle->setVisible(kind == PatternKind::Circle);
    if (kind == PatternKind::Grid && m_dir1->currentIndex() == m_dir2->currentIndex())
        setStatus(tr("Pick two different directions for the grid."), true);
}

}  // namespace forge::app
