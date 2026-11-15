// SPDX-License-Identifier: GPL-3.0-or-later
#include "MeasurePanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace forge::app {

MeasurePanel::MeasurePanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel(tr("Measure"));
    QFont bold = title->font();
    bold.setBold(true);
    title->setFont(bold);
    layout->addWidget(title);
    auto* hint = new QLabel(tr("Click a face, an edge or a corner; then a second one to see how far apart they are "
                               "and at what angle. Hold <b>Shift</b> to click an exact point instead."));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_result = new QLabel;
    m_result->setWordWrap(true);
    m_result->setTextFormat(Qt::RichText);
    m_result->setTextInteractionFlags(Qt::TextSelectableByMouse);  // to copy a number
    m_result->setMinimumHeight(120);
    m_result->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_result->setStyleSheet(QStringLiteral("QLabel { background: palette(base); border: 1px solid palette(mid); "
                                           "padding: 6px; }"));
    layout->addWidget(m_result);

    m_hover = new QLabel;
    m_hover->setWordWrap(true);
    m_hover->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(m_hover);

    auto* buttons = new QHBoxLayout;
    auto* clear = new QPushButton(tr("Clear"));
    clear->setToolTip(tr("Start again"));
    auto* done = new QPushButton(tr("Done"));
    done->setDefault(true);
    done->setToolTip(tr("Close Measure (Esc)"));
    buttons->addWidget(clear);
    buttons->addStretch(1);
    buttons->addWidget(done);
    layout->addLayout(buttons);
    layout->addStretch(1);
    connect(clear, &QPushButton::clicked, this, &MeasurePanel::clearRequested);
    connect(done, &QPushButton::clicked, this, &MeasurePanel::doneRequested);
}

void MeasurePanel::setResult(const QString& html)
{
    m_result->setText(html);
}

void MeasurePanel::setHover(const QString& text)
{
    m_hover->setText(text);
}

}  // namespace forge::app
