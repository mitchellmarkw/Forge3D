// SPDX-License-Identifier: GPL-3.0-or-later
#include "SculptPanel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

namespace forge::app {

namespace {
// A label with its combo box right under it.
QWidget* labelled(const QString& label, QWidget* field)
{
    auto* box = new QWidget;
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(2);
    auto* l = new QLabel(label);
    l->setBuddy(field);
    v->addWidget(l);
    v->addWidget(field);
    return box;
}
}  // namespace

SculptPanel::SculptPanel(QWidget* parent) : QWidget(parent)
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

    m_brush = new QComboBox;
    m_brush->addItem(tr("Draw (push out; Ctrl: push in)"));
    m_brush->addItem(tr("Smooth (even out)"));
    m_brush->setToolTip(tr("Hold Shift with any brush to smooth"));
    layout->addWidget(labelled(tr("Brush:"), m_brush));

    auto* form = new QFormLayout;
    m_size = new QDoubleSpinBox;
    m_size->setRange(0.2, 200.0);
    m_size->setDecimals(1);
    m_size->setSingleStep(0.5);
    m_size->setSuffix(tr(" mm"));
    m_size->setToolTip(tr("The brush's radius. [ and ] make it smaller and bigger."));
    form->addRow(tr("Size:"), m_size);
    auto* strengthRow = new QHBoxLayout;
    m_strength = new QSlider(Qt::Horizontal);
    m_strength->setRange(1, 100);
    m_strength->setValue(50);
    m_strengthValue = new QLabel(QStringLiteral("50"));
    m_strengthValue->setMinimumWidth(28);
    strengthRow->addWidget(m_strength, 1);
    strengthRow->addWidget(m_strengthValue);
    form->addRow(tr("Strength:"), strengthRow);
    layout->addLayout(form);

    m_detail = new QComboBox;
    m_detail->addItem(tr("Medium (about 150,000 triangles)"));
    m_detail->addItem(tr("High (about 500,000 triangles)"));
    m_detail->setToolTip(tr("How finely the shape is divided for sculpting. More is smoother, but slower. Choose "
                            "before you start."));
    layout->addWidget(labelled(tr("Detail:"), m_detail));

    auto* hint = new QLabel(tr("Drag with the left button over the shape to sculpt. Hold <b>Ctrl</b> to push in, "
                               "<b>Shift</b> to smooth. <b>[</b> and <b>]</b> change the size. Turn the view with "
                               "the right button as usual."));
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* note = new QLabel(tr("Prototype: sculpting turns the shape into a mesh, so its steps (sizes, holes, "
                               "words) can't be changed afterwards. Undo after Done puts it all back."));
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(note);

    auto* buttons = new QHBoxLayout;
    m_undo = new QPushButton(tr("Undo stroke"));
    m_undo->setToolTip(tr("Take back the last stroke (Ctrl+Z)"));
    auto* done = new QPushButton(tr("Done"));
    done->setDefault(true);
    done->setToolTip(tr("Keep the sculpted shape"));
    auto* cancel = new QPushButton(tr("Cancel"));
    cancel->setToolTip(tr("Close without changing anything (Esc)"));
    buttons->addWidget(m_undo);
    buttons->addWidget(done);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);
    layout->addStretch(1);

    connect(m_strength, &QSlider::valueChanged, this, [this](int v) { m_strengthValue->setNum(v); });
    connect(m_detail, &QComboBox::currentIndexChanged, this, &SculptPanel::detailChanged);
    connect(m_undo, &QPushButton::clicked, this, &SculptPanel::undoRequested);
    connect(done, &QPushButton::clicked, this, &SculptPanel::doneRequested);
    connect(cancel, &QPushButton::clicked, this, &SculptPanel::cancelRequested);
}

void SculptPanel::start(const QString& title, double suggestedSize)
{
    m_title->setText(title);
    const QSignalBlocker block(m_detail);
    m_size->setValue(suggestedSize);
    setDetailEnabled(true);
    setUndoEnabled(false);
}

bool SculptPanel::smoothBrush() const
{
    return m_brush->currentIndex() == 1;
}

double SculptPanel::size() const
{
    return m_size->value();
}

double SculptPanel::strength() const
{
    return m_strength->value() / 100.0;
}

SculptPanel::Detail SculptPanel::detail() const
{
    return m_detail->currentIndex() == 1 ? Detail::High : Detail::Medium;
}

void SculptPanel::setSize(double radius)
{
    m_size->setValue(radius);
}

void SculptPanel::setDetailEnabled(bool on)
{
    m_detail->setEnabled(on);
}

void SculptPanel::setUndoEnabled(bool on)
{
    m_undo->setEnabled(on);
}

void SculptPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #c0392b;") : QString());
}

}  // namespace forge::app
