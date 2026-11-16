// SPDX-License-Identifier: GPL-3.0-or-later
#include "ThreadPanel.h"

#include "forge/core/Thread.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <cmath>

namespace forge::app {

ThreadPanel::ThreadPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    m_title->setWordWrap(true);
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    layout->addWidget(m_title);
    auto* help = new QLabel(tr("Cuts a screw thread into a round face: the outside of a peg (for a bolt or a jar "
                               "neck) or the inside of a hole (for a nut). Click the round face."));
    help->setWordWrap(true);
    help->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(help);
    m_picked = new QLabel;
    m_picked->setWordWrap(true);
    layout->addWidget(m_picked);
    layout->addSpacing(6);

    // The size: its label right above it.
    layout->addWidget(new QLabel(tr("Thread size:")));
    m_standard = new QComboBox;
    for (const auto& s : thread::metric())
        m_standard->addItem(tr("%1 (pitch %2 mm)").arg(QString::fromStdString(s.name)).arg(s.pitch));
    m_standard->addItem(tr("Other size"));
    m_standard->setToolTip(tr("Standard metric threads (ISO coarse), as on bolts and nuts from the shop"));
    layout->addWidget(m_standard);

    auto* form = new QFormLayout;
    m_size = new QDoubleSpinBox;
    m_size->setRange(0.5, 500.0);
    m_size->setDecimals(2);
    m_size->setSingleStep(0.5);
    m_size->setSuffix(tr(" mm"));
    m_size->setToolTip(tr("The thread's outside diameter (a bolt's size: 6 for M6)"));
    form->addRow(tr("Diameter:"), m_size);
    m_pitch = new QDoubleSpinBox;
    m_pitch->setRange(0.1, 50.0);
    m_pitch->setDecimals(2);
    m_pitch->setSingleStep(0.05);
    m_pitch->setSuffix(tr(" mm"));
    m_pitch->setToolTip(tr("How far the thread goes along in one turn"));
    form->addRow(tr("Pitch:"), m_pitch);
    m_fit = new QDoubleSpinBox;
    m_fit->setRange(0.0, 2.0);
    m_fit->setDecimals(2);
    m_fit->setSingleStep(0.05);
    m_fit->setSuffix(tr(" mm"));
    m_fit->setValue(0.2);
    m_fit->setToolTip(tr("Room for printing: a peg's thread is made this much smaller all round, a hole's this "
                         "much bigger, so printed parts screw together. 0.1–0.3 mm suits most printers."));
    form->addRow(tr("Fit:"), m_fit);
    layout->addLayout(form);
    m_leftHand = new QCheckBox(tr("Left-hand thread (turns the other way)"));
    layout->addWidget(m_leftHand);

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

    connect(m_standard, &QComboBox::activated, this, &ThreadPanel::sizeChosen);
    for (QDoubleSpinBox* box : {m_size, m_pitch})
        connect(box, &QDoubleSpinBox::valueChanged, this, [this] {
            // Typed sizes: the list follows (a standard one, or "Other size").
            const QSignalBlocker block(m_standard);
            int match = m_standard->count() - 1;
            const auto& sizes = thread::metric();
            for (std::size_t i = 0; i < sizes.size(); ++i)
                if (std::abs(sizes[i].diameter - m_size->value()) < 1e-6 &&
                    std::abs(sizes[i].pitch - m_pitch->value()) < 1e-6)
                    match = int(i);
            m_standard->setCurrentIndex(match);
            emit changed();
        });
    connect(m_fit, &QDoubleSpinBox::valueChanged, this, &ThreadPanel::changed);
    connect(m_leftHand, &QCheckBox::toggled, this, &ThreadPanel::changed);
    connect(m_apply, &QPushButton::clicked, this, &ThreadPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &ThreadPanel::cancelRequested);
}

void ThreadPanel::sizeChosen(int index)
{
    const auto& sizes = thread::metric();
    if (index < 0 || index >= int(sizes.size()))
        return;  // "Other size": type it in
    const QSignalBlocker b0(m_size), b1(m_pitch);
    m_size->setValue(sizes[std::size_t(index)].diameter);
    m_pitch->setValue(sizes[std::size_t(index)].pitch);
    emit changed();
}

void ThreadPanel::start(const QString& title, bool editing)
{
    m_title->setText(title);
    m_apply->setToolTip(editing ? tr("Change the thread") : tr("Cut the thread"));
    setPicked({});
}

void ThreadPanel::setPicked(const QString& text)
{
    m_picked->setText(text.isEmpty() ? tr("<i>No round face picked yet.</i>") : text);
}

void ThreadPanel::setValues(double size, double pitch, double fit, bool leftHand)
{
    {
        const QSignalBlocker b0(m_size), b1(m_pitch), b2(m_fit), b3(m_leftHand);
        m_size->setValue(size);
        m_pitch->setValue(pitch);
        m_fit->setValue(fit);
        m_leftHand->setChecked(leftHand);
    }
    const QSignalBlocker block(m_standard);
    int match = m_standard->count() - 1;
    const auto& sizes = thread::metric();
    for (std::size_t i = 0; i < sizes.size(); ++i)
        if (std::abs(sizes[i].diameter - size) < 1e-6 && std::abs(sizes[i].pitch - pitch) < 1e-6)
            match = int(i);
    m_standard->setCurrentIndex(match);
}

double ThreadPanel::size() const
{
    return m_size->value();
}

double ThreadPanel::pitch() const
{
    return m_pitch->value();
}

double ThreadPanel::fit() const
{
    return m_fit->value();
}

bool ThreadPanel::leftHand() const
{
    return m_leftHand->isChecked();
}

void ThreadPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #c0392b;") : QString());
}

void ThreadPanel::setApplyEnabled(bool on)
{
    m_apply->setEnabled(on);
}

}  // namespace forge::app
