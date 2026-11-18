// SPDX-License-Identifier: GPL-3.0-or-later
#include "FitPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace forge::app {

namespace {
const char* const kAxisNames[3] = {QT_TRANSLATE_NOOP("FitPanel", "X (left to right)"),
                                   QT_TRANSLATE_NOOP("FitPanel", "Y (front to back)"),
                                   QT_TRANSLATE_NOOP("FitPanel", "Z (up)")};
const char* const kAcross[3] = {QT_TRANSLATE_NOOP("FitPanel", "across X"), QT_TRANSLATE_NOOP("FitPanel", "across Y"),
                                QT_TRANSLATE_NOOP("FitPanel", "level")};
}  // namespace

FitPanel::FitPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    m_title->setWordWrap(true);
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    layout->addWidget(m_title);
    m_summary = new QLabel;
    m_summary->setWordWrap(true);
    layout->addWidget(m_summary);
    layout->addSpacing(6);

    layout->addWidget(new QLabel(tr("Cuts (click one, or its plane in the view):")));
    m_list = new QListWidget;
    m_list->setMaximumHeight(130);
    m_list->setToolTip(tr("The cuts Split to Fit would make. Pick one to move or lean it."));
    layout->addWidget(m_list);

    auto* form = new QFormLayout;
    m_position = new QDoubleSpinBox;
    m_position->setRange(-100000.0, 100000.0);
    m_position->setDecimals(1);
    m_position->setSingleStep(1.0);
    m_position->setSuffix(tr(" mm"));
    m_position->setKeyboardTracking(false);
    m_position->setToolTip(tr("Where the cut crosses the middle of the model (plate mm). You can also drag its "
                              "plane in the view."));
    form->addRow(tr("Position:"), m_position);
    m_tilt = new QDoubleSpinBox;
    m_tilt->setRange(-45.0, 45.0);
    m_tilt->setDecimals(0);
    m_tilt->setSingleStep(5.0);
    m_tilt->setSuffix(QStringLiteral(" °"));
    m_tilt->setKeyboardTracking(false);
    m_tilt->setToolTip(tr("How far the cut leans (0: straight across)"));
    form->addRow(tr("Lean:"), m_tilt);
    layout->addLayout(form);
    // The label rests on top of its list.
    m_tiltAxisLabel = new QLabel(tr("Lean around:"));
    layout->addWidget(m_tiltAxisLabel);
    m_tiltAxis = new QComboBox;
    m_tiltAxis->setToolTip(tr("Which way the cut leans"));
    layout->addWidget(m_tiltAxis);
    layout->addSpacing(6);

    m_angled = new QCheckBox(tr("Let cuts lean (angled cuts)"));
    m_angled->setToolTip(tr("Plan again, letting each cut lean up to 30° where that cuts through less (along a "
                            "leaning arm or neck, say)"));
    layout->addWidget(m_angled);
    auto* again = new QPushButton(tr("Plan Again"));
    again->setToolTip(tr("Forget the changes and let Forge3D place the cuts again"));
    layout->addWidget(again);

    m_problems = new QLabel;
    m_problems->setWordWrap(true);
    layout->addWidget(m_problems);
    layout->addSpacing(6);
    m_layOut = new QCheckBox(tr("Then lay the pieces out on plates"));
    m_layOut->setToolTip(tr("Once cut, turn each piece onto a cut face (pegs up, holes down, so they print round) or "
                            "its flattest side, and set them out side by side on as many plates as it takes"));
    layout->addWidget(m_layOut);

    auto* help = new QLabel(tr("Each cut goes right through the model and is keyed with pegs (set in Design > "
                               "Split). One Undo puts it all back."));
    help->setWordWrap(true);
    help->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(help);

    auto* buttons = new QHBoxLayout;
    m_apply = new QPushButton(tr("Split"));
    m_apply->setDefault(true);
    m_apply->setToolTip(tr("Make these cuts"));
    auto* cancel = new QPushButton(tr("Cancel"));
    buttons->addWidget(m_apply);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);
    layout->addStretch(1);

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row == m_selected)
            return;
        m_selected = row;
        showSelected();
        emit cutSelected(row);
    });
    const auto edited = [this] {
        if (m_selected < 0 || m_selected >= int(m_cuts.size()))
            return;
        FitCut c = m_cuts[std::size_t(m_selected)];
        c.position = m_position->value();
        c.tilt = m_tilt->value();
        if (m_tiltAxis->currentIndex() >= 0)
            c.tiltAxis = m_tiltAxis->currentData().toInt();
        emit cutEdited(m_selected, c);
    };
    connect(m_position, &QDoubleSpinBox::valueChanged, this, edited);
    connect(m_tilt, &QDoubleSpinBox::valueChanged, this, edited);
    connect(m_tiltAxis, &QComboBox::currentIndexChanged, this, edited);
    connect(m_angled, &QCheckBox::toggled, this, &FitPanel::angledChanged);
    connect(m_layOut, &QCheckBox::toggled, this, &FitPanel::layOutChanged);
    connect(again, &QPushButton::clicked, this, &FitPanel::replanRequested);
    connect(m_apply, &QPushButton::clicked, this, &FitPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &FitPanel::cancelRequested);
}

void FitPanel::start(const QString& title, const QString& summary, bool angled, bool layOut)
{
    m_title->setText(title);
    m_summary->setText(summary);
    const QSignalBlocker block(m_angled), block2(m_layOut);
    m_angled->setChecked(angled);
    m_layOut->setChecked(layOut);
}

bool FitPanel::layOut() const
{
    return m_layOut->isChecked();
}

void FitPanel::setSummary(const QString& summary)
{
    m_summary->setText(summary);
}

bool FitPanel::angled() const
{
    return m_angled->isChecked();
}

void FitPanel::setCuts(const std::vector<FitCut>& cuts, int selected)
{
    m_cuts = cuts;
    m_selected = selected;
    {
        const QSignalBlocker block(m_list);
        m_list->clear();
        for (std::size_t i = 0; i < cuts.size(); ++i) {
            const FitCut& c = cuts[i];
            QString text = tr("Cut %1: %2 at %3 mm").arg(i + 1).arg(tr(kAcross[c.axis])).arg(c.position, 0, 'f', 1);
            if (c.tilt != 0.0)
                text += tr(", leaning %1° around %2").arg(c.tilt, 0, 'f', 0).arg(QChar("XYZ"[c.tiltAxis]));
            m_list->addItem(text);
        }
        m_list->setCurrentRow(selected);
    }
    showSelected();
}

void FitPanel::showSelected()
{
    const bool any = m_selected >= 0 && m_selected < int(m_cuts.size());
    for (QWidget* w : {static_cast<QWidget*>(m_position), static_cast<QWidget*>(m_tilt),
                       static_cast<QWidget*>(m_tiltAxis), static_cast<QWidget*>(m_tiltAxisLabel)})
        w->setEnabled(any);
    if (!any)
        return;
    const FitCut& c = m_cuts[std::size_t(m_selected)];
    const QSignalBlocker a(m_position), b(m_tilt), d(m_tiltAxis);
    m_position->setValue(c.position);
    m_tilt->setValue(c.tilt);
    m_tiltAxis->clear();
    for (int axis = 0; axis < 3; ++axis)
        if (axis != c.axis)
            m_tiltAxis->addItem(tr(kAxisNames[axis]), axis);
    m_tiltAxis->setCurrentIndex(m_tiltAxis->findData(c.tiltAxis));
}

void FitPanel::setProblems(const QStringList& problems)
{
    if (problems.isEmpty()) {
        m_problems->setStyleSheet(QStringLiteral("color: #2e7d32;"));
        m_problems->setText(tr("✓ Every piece will fit the printer."));
    } else {
        m_problems->setStyleSheet(QStringLiteral("color: #c0392b;"));
        m_problems->setText(QStringLiteral("⚠ ") + problems.join(QStringLiteral("\n⚠ ")));
    }
}

}  // namespace forge::app
