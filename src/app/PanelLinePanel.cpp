// SPDX-License-Identifier: GPL-3.0-or-later
#include "PanelLinePanel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include <algorithm>

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

PanelLinePanel::PanelLinePanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    m_title->setWordWrap(true);
    layout->addWidget(m_title);
    auto* hint = new QLabel(tr("Click points along the surface: the line follows it from one to the next. Hold Shift "
                               "as you click to keep it level with the last point, Ctrl to go straight up or down "
                               "from it. End Line to start another, or Close Loop to join it back to where it "
                               "began."));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_profile = new QComboBox;
    m_profile->addItem(tr("V (sharp, like a scribed line)"));
    m_profile->addItem(tr("Square"));
    m_profile->addItem(tr("Round"));
    m_profile->setToolTip(tr("The groove's cross-section"));
    layout->addWidget(labelled(tr("Groove shape:"), m_profile));
    auto* form = new QFormLayout;
    m_width = millimetres(0.1, 20.0, 0.1);
    m_width->setToolTip(tr("Across the top of the groove: 0.4 to 0.8 mm reads well on a figure"));
    m_depth = millimetres(0.05, 20.0, 0.1);
    m_depth->setToolTip(tr("How deep it goes: deep enough to hold a wash"));
    form->addRow(tr("Width:"), m_width);
    form->addRow(tr("Depth:"), m_depth);
    layout->addLayout(form);

    m_counts = new QLabel;
    layout->addWidget(m_counts);
    auto* grid = new QGridLayout;
    auto* endLine = new QPushButton(tr("End Line"));
    endLine->setToolTip(tr("Finish this line; the next click starts a new one"));
    auto* closeLoop = new QPushButton(tr("Close Loop"));
    closeLoop->setToolTip(tr("Join this line back to its first point (round a panel or a plate)"));
    auto* undoPoint = new QPushButton(tr("Undo Point"));
    auto* removeLine = new QPushButton(tr("Remove Line"));
    removeLine->setToolTip(tr("Take away the line being drawn, or else the last finished one"));
    auto* clear = new QPushButton(tr("Clear"));
    grid->addWidget(endLine, 0, 0);
    grid->addWidget(closeLoop, 0, 1);
    grid->addWidget(undoPoint, 1, 0);
    grid->addWidget(removeLine, 1, 1);
    grid->addWidget(clear, 2, 0);
    layout->addLayout(grid);

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

    const auto remember = [this] {
        if (!m_profile->property("editing").toBool()) {
            QSettings p = preferences();
            p.setValue(QStringLiteral("panelLines/profile"), m_profile->currentIndex());
            p.setValue(QStringLiteral("panelLines/width"), m_width->value());
            p.setValue(QStringLiteral("panelLines/depth"), m_depth->value());
        }
        emit changed();
    };
    connect(m_profile, &QComboBox::currentIndexChanged, this, remember);
    connect(m_width, &QDoubleSpinBox::valueChanged, this, remember);
    connect(m_depth, &QDoubleSpinBox::valueChanged, this, remember);
    connect(endLine, &QPushButton::clicked, this, &PanelLinePanel::endLineRequested);
    connect(closeLoop, &QPushButton::clicked, this, &PanelLinePanel::closeLoopRequested);
    connect(undoPoint, &QPushButton::clicked, this, &PanelLinePanel::undoPointRequested);
    connect(removeLine, &QPushButton::clicked, this, &PanelLinePanel::removeLineRequested);
    connect(clear, &QPushButton::clicked, this, &PanelLinePanel::clearRequested);
    connect(m_apply, &QPushButton::clicked, this, &PanelLinePanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &PanelLinePanel::cancelRequested);
}

void PanelLinePanel::start(const QString& title, const PanelLineFinish* editing)
{
    m_title->setText(title);
    const QSignalBlocker a(m_profile), b(m_width), c(m_depth);
    m_profile->setProperty("editing", editing != nullptr);
    if (editing) {
        m_profile->setCurrentIndex(int(editing->profile));
        m_width->setValue(editing->width);
        m_depth->setValue(editing->depth);
    } else {
        const QSettings prefs = preferences();
        m_profile->setCurrentIndex(std::clamp(prefs.value(QStringLiteral("panelLines/profile"), 0).toInt(), 0, 2));
        m_width->setValue(prefs.value(QStringLiteral("panelLines/width"), 0.6).toDouble());
        m_depth->setValue(prefs.value(QStringLiteral("panelLines/depth"), 0.5).toDouble());
    }
    setStatus({}, false);
}

PanelLineFinish PanelLinePanel::settings() const
{
    PanelLineFinish f;
    f.profile = GrooveProfile(m_profile->currentIndex());
    f.width = m_width->value();
    f.depth = m_depth->value();
    return f;
}

void PanelLinePanel::setCounts(int lines, int points)
{
    QString text = lines == 0 ? tr("No lines yet.") : lines == 1 ? tr("1 line.") : tr("%1 lines.").arg(lines);
    if (points > 0)
        text += QLatin1Char(' ') + (points == 1 ? tr("Drawing one: 1 point (click the next).")
                                                : tr("Drawing one: %1 points.").arg(points));
    m_counts->setText(text);
    m_apply->setEnabled(lines > 0 || points >= 2);
}

void PanelLinePanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #d04020;") : QString());
    m_status->setVisible(!text.isEmpty());
}

}  // namespace forge::app
