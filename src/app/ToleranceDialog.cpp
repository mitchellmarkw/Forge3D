// SPDX-License-Identifier: GPL-3.0-or-later
#include "ToleranceDialog.h"

#include "Fits.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStringList>
#include <QVBoxLayout>

namespace forge::app {
namespace {

QSettings preferences()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"), QStringLiteral("Forge3D"));
}

QDoubleSpinBox* millimetres(double low, double high, double step, double value)
{
    auto* box = new QDoubleSpinBox;
    box->setRange(low, high);
    box->setDecimals(2);
    box->setSingleStep(step);
    box->setSuffix(QObject::tr(" mm"));
    box->setValue(value);
    return box;
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

const tolerance::Kind kKinds[3] = {tolerance::Kind::Peg, tolerance::Kind::Ball, tolerance::Kind::Pin};

}  // namespace

ToleranceDialog::ToleranceDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Tolerance Test Print"));
    auto* layout = new QVBoxLayout(this);
    auto* help = new QLabel(tr("Find the fit that suits your printer, once. <b>1.</b> Add the test to the plate and "
                               "print it. <b>2.</b> Try the pegs, the ball and the pins in each hole (the numbers "
                               "are the fit: .15 is 0.15 mm of room all round). <b>3.</b> Record the one that fits "
                               "best for each: Quick Socket and Quick Hinge start from it."));
    help->setWordWrap(true);
    layout->addWidget(help);

    // 1. Make
    auto* make = new QGroupBox(tr("1. Make the test"));
    auto* grid = new QGridLayout(make);
    const QSettings p0 = preferences();
    m_pegs = new QCheckBox(tr("Pegs in holes"));
    m_pegSize = millimetres(1.0, 50.0, 0.5, p0.value(QStringLiteral("tolerance/pegSize"), 3.0).toDouble());
    m_pegDepth = millimetres(1.0, 50.0, 0.5, p0.value(QStringLiteral("tolerance/pegDepth"), 4.0).toDouble());
    m_pegSize->setToolTip(tr("The peg's diameter: use the size you use most"));
    m_pegDepth->setToolTip(tr("How far the peg goes in"));
    m_balls = new QCheckBox(tr("A ball in snap-in cups"));
    m_ballSize = millimetres(2.0, 50.0, 0.5, p0.value(QStringLiteral("tolerance/ballSize"), 6.0).toDouble());
    m_ballSize->setToolTip(tr("The ball's diameter"));
    m_pins = new QCheckBox(tr("Hinge pins through holes"));
    m_pinSize = millimetres(0.5, 20.0, 0.25, p0.value(QStringLiteral("tolerance/pinSize"), 2.0).toDouble());
    m_pinSize->setToolTip(tr("The pin's diameter (a printed pin)"));
    m_pegs->setChecked(p0.value(QStringLiteral("tolerance/pegs"), true).toBool());
    m_balls->setChecked(p0.value(QStringLiteral("tolerance/balls"), true).toBool());
    m_pins->setChecked(p0.value(QStringLiteral("tolerance/pins"), true).toBool());
    int row = 0;
    grid->addWidget(m_pegs, row, 0);
    grid->addWidget(new QLabel(tr("Across:")), row, 1);
    grid->addWidget(m_pegSize, row, 2);
    grid->addWidget(new QLabel(tr("Goes in:")), row, 3);
    grid->addWidget(m_pegDepth, row++, 4);
    grid->addWidget(m_balls, row, 0);
    grid->addWidget(new QLabel(tr("Across:")), row, 1);
    grid->addWidget(m_ballSize, row++, 2);
    grid->addWidget(m_pins, row, 0);
    grid->addWidget(new QLabel(tr("Across:")), row, 1);
    grid->addWidget(m_pinSize, row++, 2);
    auto* fitsRow = new QHBoxLayout;
    m_from = millimetres(0.0, 2.0, 0.05, p0.value(QStringLiteral("tolerance/from"), 0.10).toDouble());
    m_to = millimetres(0.0, 2.0, 0.05, p0.value(QStringLiteral("tolerance/to"), 0.30).toDouble());
    m_step = millimetres(0.01, 1.0, 0.01, p0.value(QStringLiteral("tolerance/step"), 0.05).toDouble());
    fitsRow->addWidget(new QLabel(tr("Fits from")));
    fitsRow->addWidget(m_from);
    fitsRow->addWidget(new QLabel(tr("to")));
    fitsRow->addWidget(m_to);
    fitsRow->addWidget(new QLabel(tr("every")));
    fitsRow->addWidget(m_step);
    grid->addLayout(fitsRow, row++, 0, 1, 5);
    m_fitsText = new QLabel;
    m_fitsText->setWordWrap(true);
    grid->addWidget(m_fitsText, row++, 0, 1, 5);
    auto* add = new QPushButton(tr("Add to the Plate"));
    add->setToolTip(tr("Add the test bars and the pieces to try in them, ready to print"));
    grid->addWidget(add, row++, 0, 1, 5, Qt::AlignLeft);
    layout->addWidget(make);

    // 2. Record
    auto* record = new QGroupBox(tr("2. Record what fit best"));
    auto* recordGrid = new QGridLayout(record);
    m_printer = new QComboBox;
    m_printer->addItem(tr("Resin"));
    m_printer->addItem(tr("FDM (filament)"));
    m_printer->setToolTip(tr("The printer the test was printed on: each printer keeps its own fits"));
    recordGrid->addWidget(labelled(tr("Printer:"), m_printer), 0, 0, 1, 2);
    const QString names[3] = {tr("Pegs (Quick Socket, and a swivel's peg):"), tr("Balls (Quick Socket):"),
                              tr("Hinge pins (Quick Hinge):")};
    for (int i = 0; i < 3; ++i) {
        m_best[i] = millimetres(-0.01, 2.0, 0.05, -0.01);
        m_best[i]->setSpecialValueText(tr("Not tested"));
        m_best[i]->setToolTip(tr("The label on the hole that fit best ('Not tested' to keep the usual fit)"));
        recordGrid->addWidget(new QLabel(names[i]), i + 1, 0);
        recordGrid->addWidget(m_best[i], i + 1, 1);
    }
    auto* save = new QPushButton(tr("Use These Fits"));
    save->setToolTip(tr("Quick Socket and Quick Hinge start from these fits for this printer (you can still "
                        "change the fit each time)"));
    recordGrid->addWidget(save, 4, 0, 1, 2, Qt::AlignLeft);
    layout->addWidget(record);

    m_status = new QLabel;
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    auto* close = new QPushButton(tr("Close"));
    auto* closeRow = new QHBoxLayout;
    closeRow->addStretch(1);
    closeRow->addWidget(close);
    layout->addLayout(closeRow);

    m_printer->setCurrentIndex(p0.value(QStringLiteral("socket/printer"), QStringLiteral("resin")).toString() ==
                                       QStringLiteral("fdm")
                                   ? 1
                                   : 0);
    showRecorded();
    showFits();
    for (QDoubleSpinBox* box : {m_from, m_to, m_step})
        connect(box, &QDoubleSpinBox::valueChanged, this, &ToleranceDialog::showFits);
    connect(m_printer, &QComboBox::currentIndexChanged, this, &ToleranceDialog::showRecorded);
    connect(add, &QPushButton::clicked, this, [this] {
        tolerance::Settings s;
        s.pegs = m_pegs->isChecked();
        s.balls = m_balls->isChecked();
        s.pins = m_pins->isChecked();
        s.pegSize = m_pegSize->value();
        s.pegDepth = m_pegDepth->value();
        s.ballSize = m_ballSize->value();
        s.pinSize = m_pinSize->value();
        try {
            s.fits = tolerance::steps(m_from->value(), m_to->value(), m_step->value());
            tolerance::check(s);
        } catch (const std::exception& e) {
            m_status->setText(QString::fromUtf8(e.what()));
            return;
        }
        auto p = preferences();
        p.setValue(QStringLiteral("tolerance/pegs"), s.pegs);
        p.setValue(QStringLiteral("tolerance/balls"), s.balls);
        p.setValue(QStringLiteral("tolerance/pins"), s.pins);
        p.setValue(QStringLiteral("tolerance/pegSize"), s.pegSize);
        p.setValue(QStringLiteral("tolerance/pegDepth"), s.pegDepth);
        p.setValue(QStringLiteral("tolerance/ballSize"), s.ballSize);
        p.setValue(QStringLiteral("tolerance/pinSize"), s.pinSize);
        p.setValue(QStringLiteral("tolerance/from"), m_from->value());
        p.setValue(QStringLiteral("tolerance/to"), m_to->value());
        p.setValue(QStringLiteral("tolerance/step"), m_step->value());
        m_status->setText(tr("Added to the plate. Print it, then come back here to record what fit best."));
        emit addRequested(s);
    });
    connect(save, &QPushButton::clicked, this, [this] {
        const bool resin = m_printer->currentIndex() == 0;
        QStringList kept;
        for (int i = 0; i < 3; ++i) {
            const double v = m_best[i]->value();
            const std::optional<double> fit = v < 0.0 ? std::nullopt : std::optional<double>(v);
            fits::setMeasured(resin, kKinds[i], fit);
            if (fit)
                kept << QStringLiteral("%1 mm").arg(*fit, 0, 'f', 2);
        }
        auto p = preferences();
        // Start from them next time, if this is the printer last used.
        const QString printer = resin ? QStringLiteral("resin") : QStringLiteral("fdm");
        if (p.value(QStringLiteral("socket/printer"), QStringLiteral("resin")).toString() == printer) {
            const bool peg = p.value(QStringLiteral("socket/kind")).toString() == QStringLiteral("peg");
            p.setValue(QStringLiteral("socket/fit"), fits::starting(resin, peg ? tolerance::Kind::Peg : tolerance::Kind::Ball));
        }
        if (p.value(QStringLiteral("hinge/printer"), QStringLiteral("resin")).toString() == printer)
            p.setValue(QStringLiteral("hinge/fit"), fits::starting(resin, tolerance::Kind::Pin));
        // Split for Printing's pegs too.
        if (const auto peg = fits::measured(resin, tolerance::Kind::Peg))
            p.setValue(QStringLiteral("split/clearance"), *peg);
        m_status->setText(kept.isEmpty() ? tr("Back to the usual fits for this printer.")
                                         : tr("Saved. Quick Socket and Quick Hinge start from these fits for this "
                                              "printer; Split for Printing's pegs use the peg fit."));
    });
    connect(close, &QPushButton::clicked, this, &QDialog::close);
}

void ToleranceDialog::showRecorded()
{
    const bool resin = m_printer->currentIndex() == 0;
    for (int i = 0; i < 3; ++i)
        m_best[i]->setValue(fits::measured(resin, kKinds[i]).value_or(-0.01));
}

void ToleranceDialog::showFits()
{
    try {
        const auto list = tolerance::steps(m_from->value(), m_to->value(), m_step->value());
        QStringList labels;
        for (double f : list)
            labels << QString::fromStdString(tolerance::label(f));
        m_fitsText->setText(tr("Holes labelled: %1").arg(labels.join(QStringLiteral("  "))));
        m_fitsText->setStyleSheet(QString());
    } catch (const std::exception& e) {
        m_fitsText->setText(QString::fromUtf8(e.what()));
        m_fitsText->setStyleSheet(QStringLiteral("color: #c0392b;"));
    }
}

}  // namespace forge::app
