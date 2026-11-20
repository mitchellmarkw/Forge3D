// SPDX-License-Identifier: GPL-3.0-or-later
#include "HollowPanel.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace forge::app {
namespace {

QSettings preferences()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"), QStringLiteral("Forge3D"));
}

QDoubleSpinBox* millimetres(double low, double high, double step)
{
    auto* box = new QDoubleSpinBox;
    box->setRange(low, high);
    box->setDecimals(2);
    box->setSingleStep(step);
    box->setSuffix(QObject::tr(" mm"));
    box->setKeyboardTracking(false);
    return box;
}

}  // namespace

HollowPanel::HollowPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    m_title->setWordWrap(true);
    layout->addWidget(m_title);
    auto* hint = new QLabel(tr("For resin printing: the inside taken out, leaving walls this thick, with drain holes "
                               "so the resin inside can run out. Works on any shape, sculpts and imported meshes "
                               "too."));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* form = new QFormLayout;
    m_thickness = millimetres(0.5, 20.0, 0.25);
    m_thickness->setToolTip(tr("How thick the walls are left: about 1.5 to 2.5 mm for resin figures"));
    form->addRow(tr("Wall thickness:"), m_thickness);
    layout->addLayout(form);

    auto* holesBox = new QGroupBox(tr("Drain holes"));
    auto* holes = new QVBoxLayout(holesBox);
    auto* holesHint = new QLabel(tr("Click the shape where a hole goes: low down and out of sight is best (where it "
                                    "meets the build plate, or underneath). Two or more let air in as resin runs "
                                    "out."));
    holesHint->setWordWrap(true);
    holes->addWidget(holesHint);
    auto* sizeForm = new QFormLayout;
    m_holeSize = millimetres(0.5, 30.0, 0.5);
    m_holeSize->setToolTip(tr("The holes' diameter: 2.5 to 4 mm drains well"));
    sizeForm->addRow(tr("Hole size:"), m_holeSize);
    holes->addLayout(sizeForm);
    m_holes = new QLabel;
    holes->addWidget(m_holes);
    auto* holeButtons = new QHBoxLayout;
    auto* place = new QPushButton(tr("Place Them for Me"));
    place->setToolTip(tr("Two holes at the lowest points of the shape as it sits, apart from each other (instead "
                         "of the ones there)"));
    auto* removeLast = new QPushButton(tr("Remove Last"));
    auto* clear = new QPushButton(tr("Clear"));
    holeButtons->addWidget(place);
    holeButtons->addWidget(removeLast);
    holeButtons->addWidget(clear);
    holes->addLayout(holeButtons);
    m_plugs = new QCheckBox(tr("Make plugs to close the holes"));
    m_plugs->setToolTip(tr("Short pegs the size of the holes (less the fit for resin), beside the model, to glue "
                           "in after printing and cleaning"));
    holes->addWidget(m_plugs);
    layout->addWidget(holesBox);

    m_status = new QLabel;
    m_status->setWordWrap(true);
    m_status->hide();
    layout->addWidget(m_status);

    auto* buttons = new QHBoxLayout;
    auto* cancel = new QPushButton(tr("Cancel"));
    m_apply = new QPushButton(tr("Apply"));
    m_apply->setDefault(true);
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(m_apply);
    layout->addLayout(buttons);
    layout->addStretch(1);

    const QSettings prefs = preferences();
    m_thickness->setValue(prefs.value(QStringLiteral("hollow/thickness"), 2.0).toDouble());
    m_holeSize->setValue(prefs.value(QStringLiteral("hollow/holeSize"), 3.0).toDouble());
    m_plugs->setChecked(prefs.value(QStringLiteral("hollow/plugs"), true).toBool());

    const auto remember = [this] {
        QSettings p = preferences();
        p.setValue(QStringLiteral("hollow/thickness"), m_thickness->value());
        p.setValue(QStringLiteral("hollow/holeSize"), m_holeSize->value());
        p.setValue(QStringLiteral("hollow/plugs"), m_plugs->isChecked());
    };
    for (QDoubleSpinBox* box : {m_thickness, m_holeSize})
        connect(box, &QDoubleSpinBox::valueChanged, this, [this, remember] {
            remember();
            emit changed();
        });
    connect(m_plugs, &QCheckBox::toggled, this, remember);
    connect(place, &QPushButton::clicked, this, &HollowPanel::placeRequested);
    connect(removeLast, &QPushButton::clicked, this, &HollowPanel::removeLastRequested);
    connect(clear, &QPushButton::clicked, this, &HollowPanel::clearRequested);
    connect(m_apply, &QPushButton::clicked, this, &HollowPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &HollowPanel::cancelRequested);
}

void HollowPanel::start(const QString& title, bool editing, double thickness, double holeSize)
{
    m_title->setText(title);
    const QSignalBlocker a(m_thickness), b(m_holeSize);
    if (editing) {
        m_thickness->setValue(thickness);
        m_holeSize->setValue(holeSize);
    }
    m_plugs->setVisible(!editing);  // plugs are made once, with the first holes
    setStatus({}, false);
}

double HollowPanel::thickness() const
{
    return m_thickness->value();
}

double HollowPanel::holeSize() const
{
    return m_holeSize->value();
}

bool HollowPanel::plugs() const
{
    return m_plugs->isVisible() && m_plugs->isChecked();
}

void HollowPanel::setHoleCount(int count)
{
    m_holes->setText(count == 0   ? tr("No holes yet: the resin inside would stay there.")
                     : count == 1 ? tr("1 hole (add another, to let air in).")
                                  : tr("%1 holes.").arg(count));
    m_holes->setStyleSheet(count < 2 ? QStringLiteral("color: #c07000;") : QString());
}

void HollowPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #d04020;") : QString());
    m_status->setVisible(!text.isEmpty());
}

void HollowPanel::setApplyEnabled(bool on)
{
    m_apply->setEnabled(on);
}

}  // namespace forge::app
