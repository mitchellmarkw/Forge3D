// SPDX-License-Identifier: GPL-3.0-or-later
#include "RotateForPrintDialog.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace forge::app {

namespace {

const orient::Goal kGoals[] = {orient::Goal::Fastest, orient::Goal::Quality, orient::Goal::Strongest};

QString number(double v, int decimals)
{
    return QString::number(v, 'f', decimals);
}

}  // namespace

RotateForPrintDialog::RotateForPrintDialog(int shapes, std::function<orient::Choice(orient::Goal)> work,
                                           QWidget* parent)
    : QDialog(parent), m_work(std::move(work))
{
    setWindowTitle(tr("Rotate for Best Print"));
    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(shapes == 1 ? tr("Turns the shape the best way up to print, and rests it on the plate.")
                                         : tr("Turns each of the %1 shapes the best way up to print, and rests them "
                                              "on the plate.")
                                               .arg(shapes));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* goalColumn = new QVBoxLayout;
    goalColumn->setSpacing(0);
    goalColumn->addWidget(new QLabel(tr("Best for:")));
    m_goal = new QComboBox;
    m_goal->addItem(tr("Fastest Print"));
    m_goal->addItem(tr("Best Quality"));
    m_goal->addItem(tr("Strongest Print"));
    goalColumn->addWidget(m_goal);
    layout->addLayout(goalColumn);
    m_about = new QLabel;
    m_about->setWordWrap(true);
    m_about->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(m_about);

    m_table = new QTableWidget(6, 2);
    m_table->setHorizontalHeaderLabels({tr("Now"), tr("Turned")});
    m_table->setVerticalHeaderLabels({tr("Height (layers)"), tr("Support needed"), tr("Surface supports touch"),
                                      tr("Flat on the plate"), tr("Sloping surface (shows layers)"),
                                      tr("Length standing up")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_table);
    m_verdict = new QLabel;
    m_verdict->setWordWrap(true);
    layout->addWidget(m_verdict);

    auto* buttons = new QDialogButtonBox;
    QPushButton* rotate = buttons->addButton(tr("Rotate"), QDialogButtonBox::AcceptRole);
    rotate->setDefault(true);
    buttons->addButton(QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    {
        const QSettings s(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"),
                          QStringLiteral("Forge3D"));
        m_goal->setCurrentIndex(std::clamp(s.value(QStringLiteral("orient/goal"), 0).toInt(), 0, 2));
    }
    connect(m_goal, &QComboBox::currentIndexChanged, this, [this](int i) {
        QSettings s(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"), QStringLiteral("Forge3D"));
        s.setValue(QStringLiteral("orient/goal"), i);
        refresh();
    });
    refresh();
    resize(460, sizeHint().height());
}

orient::Goal RotateForPrintDialog::goal() const
{
    return kGoals[std::clamp(m_goal->currentIndex(), 0, 2)];
}

void RotateForPrintDialog::refresh()
{
    switch (goal()) {
    case orient::Goal::Fastest:
        m_about->setText(tr("The fewest layers (lowest height) and the least support to print, standing steady."));
        break;
    case orient::Goal::Quality:
        m_about->setText(tr("The least surface marked by supports, and the least gently sloping surface (where "
                            "layer lines show most), even if it prints taller."));
        break;
    case orient::Goal::Strongest:
        m_about->setText(tr("Lying along its length, so the layers run along it: a part breaks between layers, so "
                            "a limb or blade printed standing up snaps easily. Then on a steady base."));
        break;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    orient::Choice c;
    QString problem;
    try {
        c = m_work(goal());
    } catch (const std::exception& e) {
        problem = QString::fromUtf8(e.what());
    }
    QApplication::restoreOverrideCursor();
    if (!problem.isEmpty()) {
        m_verdict->setText(problem);
        return;
    }
    const auto row = [&](int r, double now, double turned, const QString& unit, int decimals) {
        m_table->setItem(r, 0, new QTableWidgetItem(number(now, decimals) + unit));
        m_table->setItem(r, 1, new QTableWidgetItem(number(turned, decimals) + unit));
        for (int col = 0; col < 2; ++col)
            m_table->item(r, col)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    };
    row(0, c.now.height, c.best.height, tr(" mm"), 1);
    row(1, c.now.supportVolume / 1000.0, c.best.supportVolume / 1000.0, tr(" cm³"), 2);
    row(2, c.now.overhangArea, c.best.overhangArea, tr(" mm²"), 0);
    row(3, c.now.baseArea, c.best.baseArea, tr(" mm²"), 0);
    row(4, c.now.staircase, c.best.staircase, tr(" mm²"), 0);
    row(5, 100.0 * c.now.alongLength, 100.0 * c.best.alongLength, tr(" %"), 0);
    m_table->resizeRowsToContents();
    m_table->setFixedHeight(m_table->verticalHeader()->length() + m_table->horizontalHeader()->sizeHint().height() +
                            2 * m_table->frameWidth());
    const bool same = std::abs(c.now.down[0] - c.best.down[0]) + std::abs(c.now.down[1] - c.best.down[1]) +
                          std::abs(c.now.down[2] - c.best.down[2]) <
                      1e-6;
    m_verdict->setText(same ? tr("It's already the best way up for this: Rotate just rests it on the plate.")
                            : tr("Turned, it scores %1% better for this.")
                                  .arg(c.now.score > 0 ? number(100.0 * (1.0 - c.best.score / c.now.score), 0)
                                                       : QStringLiteral("100")));
}

}  // namespace forge::app
