// SPDX-License-Identifier: GPL-3.0-or-later
#include "DisplayBaseDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <stdexcept>

namespace forge::app {

namespace {

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

QDoubleSpinBox* spin(double lo, double hi, double value, int decimals, double step, const QString& tip)
{
    auto* s = new QDoubleSpinBox;
    s->setRange(lo, hi);
    s->setDecimals(decimals);
    s->setSingleStep(step);
    s->setSuffix(QStringLiteral(" mm"));
    s->setValue(value);
    s->setToolTip(tip);
    return s;
}

}  // namespace

DisplayBaseDialog::DisplayBaseDialog(const displaybase::Settings& start, bool holesInFeet, double holeDepth,
                                     QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Make a Display Base"));
    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("A base for the figure as it's posed now, reaching round its feet, with a peg "
                                "standing up right under each foot that touches the ground."));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_shape = new QComboBox;
    for (int i = 0; i < displaybase::kShapeCount; ++i)
        m_shape->addItem(tr(displaybase::title(displaybase::Shape(i))));
    m_shape->setCurrentIndex(int(start.shape));
    m_shape->setToolTip(tr("Round and hexagonal bases are the same size every way; oval and square ones follow "
                           "the stance"));
    layout->addWidget(labelled(tr("Shape:"), m_shape));

    auto* form = new QFormLayout;
    m_margin = spin(0.0, 200.0, start.margin, 1, 1.0, tr("How far the base reaches beyond the feet all round"));
    form->addRow(tr("Margin round the feet:"), m_margin);
    m_thickness = spin(1.0, 100.0, start.thickness, 1, 0.5, tr("How thick the base is"));
    form->addRow(tr("Thickness:"), m_thickness);
    m_bevel = spin(0.0, 80.0, start.bevel, 1, 0.25, tr("How much is taken off the top edge (0: a square edge)"));
    form->addRow(tr("Bevel on the top edge:"), m_bevel);
    layout->addLayout(form);

    m_kind = new QComboBox;
    m_kind->addItem(tr("Pegs standing up (holes in the feet)"));
    m_kind->addItem(tr("Holes going down (pegs on the feet)"));
    m_kind->setCurrentIndex(start.pegs ? 0 : 1);
    layout->addWidget(labelled(tr("Under each foot:"), m_kind));

    auto* pegForm = new QFormLayout;
    m_pegSize = spin(0.5, 30.0, start.pegSize, 2, 0.1, tr("The diameter of the pegs (the same as the feet's holes "
                                                          "or pegs)"));
    m_pegSize->setPrefix(tr("Ø "));
    pegForm->addRow(tr("Peg diameter:"), m_pegSize);
    m_pegHeight = spin(0.5, 50.0, start.pegHeight, 1, 0.5, {});
    m_pegHeightLabel = new QLabel;
    pegForm->addRow(m_pegHeightLabel, m_pegHeight);
    m_fit = spin(0.0, 1.0, start.fit, 2, 0.05, tr("Room all round a foot's peg in the base's holes"));
    m_fitLabel = new QLabel(tr("Fit:"));
    pegForm->addRow(m_fitLabel, m_fit);
    layout->addLayout(pegForm);

    m_holes = new QCheckBox(tr("Make the holes in the feet too"));
    m_holes->setToolTip(tr("Stand peg holes straight up into the middle of each sole that touches the ground, where "
                           "a foot doesn't have one already (Quick Socket steps, with your printer's fit)"));
    m_holes->setChecked(holesInFeet);
    layout->addWidget(m_holes);
    auto* holeForm = new QFormLayout;
    m_holeDepth = spin(0.5, 40.0, holeDepth, 1, 0.5, tr("How far the holes go up into the feet"));
    m_holeDepthLabel = new QLabel(tr("Hole depth in the feet:"));
    holeForm->addRow(m_holeDepthLabel, m_holeDepth);
    layout->addLayout(holeForm);

    m_note = new QLabel;
    m_note->setWordWrap(true);
    layout->addWidget(m_note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Make Base"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    m_ok = buttons->button(QDialogButtonBox::Ok);

    for (auto* s : {m_margin, m_thickness, m_bevel, m_pegSize, m_pegHeight, m_fit, m_holeDepth})
        connect(s, &QDoubleSpinBox::valueChanged, this, &DisplayBaseDialog::refresh);
    connect(m_kind, &QComboBox::currentIndexChanged, this, &DisplayBaseDialog::refresh);
    connect(m_holes, &QCheckBox::toggled, this, &DisplayBaseDialog::refresh);
    refresh();
}

void DisplayBaseDialog::refresh()
{
    const bool pegs = m_kind->currentIndex() == 0;
    m_pegHeightLabel->setText(pegs ? tr("Peg height:") : tr("Hole depth:"));
    m_pegHeight->setToolTip(pegs ? tr("How far the pegs stand up out of the base (a little less than the feet's "
                                      "holes are deep)")
                                 : tr("How far the holes go down into the base"));
    m_fitLabel->setVisible(!pegs);
    m_fit->setVisible(!pegs);
    m_holes->setVisible(pegs);
    m_holeDepthLabel->setVisible(pegs && m_holes->isChecked());
    m_holeDepth->setVisible(pegs && m_holes->isChecked());
    QString problem;
    try {
        displaybase::check(settings());
        if (pegs && m_holes->isChecked() && m_pegHeight->value() > m_holeDepth->value())
            problem = tr("The pegs stand taller than the holes in the feet are deep, so the feet won't sit down on "
                         "the base.");
    } catch (const std::exception& e) {
        problem = QString::fromUtf8(e.what());
    }
    m_note->setText(problem);
    m_note->setVisible(!problem.isEmpty());
    m_ok->setEnabled(problem.isEmpty() || problem.startsWith(tr("The pegs stand taller")));
    adjustSize();
}

displaybase::Settings DisplayBaseDialog::settings() const
{
    displaybase::Settings s;
    s.shape = displaybase::Shape(m_shape->currentIndex());
    s.margin = m_margin->value();
    s.thickness = m_thickness->value();
    s.bevel = m_bevel->value();
    s.pegs = m_kind->currentIndex() == 0;
    s.pegSize = m_pegSize->value();
    s.pegHeight = m_pegHeight->value();
    s.fit = m_fit->value();
    return s;
}

bool DisplayBaseDialog::holesInFeet() const
{
    return m_kind->currentIndex() == 0 && m_holes->isChecked();
}

double DisplayBaseDialog::holeDepth() const
{
    return m_holeDepth->value();
}

}  // namespace forge::app
