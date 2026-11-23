// SPDX-License-Identifier: GPL-3.0-or-later
#include "PortPanel.h"

#include "FormulaFields.h"

#include "Standards.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QVBoxLayout>

namespace forge::app {
namespace {

QSettings preferences()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"), QStringLiteral("Forge3D"));
}

QDoubleSpinBox* spin(double low, double high, double step, const QString& suffix)
{
    auto* box = new QDoubleSpinBox;
    box->setRange(low, high);
    box->setDecimals(2);
    box->setSingleStep(step);
    box->setSuffix(suffix);
    box->setKeyboardTracking(false);
    return box;
}

// A combo box with its label right on top of it.
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

QLabel* wrapped(const QString& text)
{
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    return label;
}

}  // namespace

PortPanel::PortPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    m_title->setWordWrap(true);
    layout->addWidget(m_title);
    layout->addWidget(wrapped(tr("Swappable hands and accessories: a peg on the arm, the socket for it in each hand, "
                                 "and grip holes through hands sized to your weapons' handles.")));

    m_standard = new StandardRow;
    layout->addWidget(m_standard);

    m_what = new QComboBox;
    m_what->addItem(tr("A peg (on an arm or an accessory)"));
    m_what->addItem(tr("The socket for the peg (in a hand)"));
    m_what->addItem(tr("A grip hole (through a hand, for a handle)"));
    m_what->setToolTip(tr("Make the peg on the arm, then the socket in each hand with the same sizes"));
    layout->addWidget(labelled(tr("What to add:"), m_what));

    // The peg (its sizes are the socket's too).
    auto* peg = new QGroupBox(tr("Peg"));
    m_pegBox = peg;
    auto* pegLayout = new QVBoxLayout(peg);
    m_shape = new QComboBox;
    m_shape->addItem(tr("Round"));
    m_shape->addItem(tr("Square"));
    m_shape->addItem(tr("Hexagon"));
    m_shape->addItem(tr("Octagon"));
    m_shape->setToolTip(tr("Round turns in its socket; the others hold the hand at the angle it's put in"));
    pegLayout->addWidget(labelled(tr("Shape:"), m_shape));
    auto* pegForm = new QFormLayout;
    m_size = spin(0.5, 50.0, 0.1, tr(" mm"));
    m_size->setToolTip(tr("The peg's diameter (round) or across its flats"));
    m_sizeLabel = new QLabel(tr("Diameter:"));
    pegForm->addRow(m_sizeLabel, m_size);
    m_length = spin(0.5, 100.0, 0.5, tr(" mm"));
    m_length->setToolTip(tr("How far the peg stands out (with a ball: to the ball's middle); the socket is as deep"));
    pegForm->addRow(tr("Length:"), m_length);
    pegLayout->addLayout(pegForm);
    m_end = new QComboBox;
    m_end->addItem(tr("Flat"));
    m_end->addItem(tr("Chamfered"));
    m_end->addItem(tr("Ball (for a ball-jointed wrist)"));
    m_end->setToolTip(tr("Chamfered: the edge taken off, so it pushes in easily. Ball: the hand takes a snap-in ball "
                         "socket and swivels on it"));
    pegLayout->addWidget(labelled(tr("End:"), m_end));
    m_ballRow = new QWidget;
    auto* ballForm = new QFormLayout(m_ballRow);
    ballForm->setContentsMargins(0, 0, 0, 0);
    m_ball = spin(0.5, 60.0, 0.1, tr(" mm"));
    m_ball->setToolTip(tr("The ball's diameter"));
    ballForm->addRow(tr("Ball size:"), m_ball);
    pegLayout->addWidget(m_ballRow);
    auto* turnForm = new QFormLayout;
    m_turn = spin(-180.0, 180.0, 15.0, tr("°"));
    m_turn->setToolTip(tr("Turns a shaped peg round its axis, so its flats line up"));
    turnForm->addRow(tr("Turned:"), m_turn);
    pegLayout->addLayout(turnForm);
    layout->addWidget(peg);

    auto* socket = new QGroupBox(tr("Socket"));
    m_socketBox = socket;
    auto* socketLayout = new QVBoxLayout(socket);
    socketLayout->addWidget(wrapped(tr("Takes a peg of the sizes above, with room for printing.")));
    m_printer = new QComboBox;
    m_printer->addItem(tr("Resin"));
    m_printer->addItem(tr("FDM (filament)"));
    m_printer->setToolTip(tr("The printer the hand is printed on: it sets the fit to start from"));
    socketLayout->addWidget(labelled(tr("Printed on:"), m_printer));
    auto* socketForm = new QFormLayout;
    m_socketFit = spin(0.0, 1.0, 0.05, tr(" mm"));
    m_socketFit->setToolTip(tr("Room all round the peg (Tools → Tolerance Test finds yours)"));
    socketForm->addRow(tr("Fit:"), m_socketFit);
    socketLayout->addLayout(socketForm);
    layout->addWidget(socket);

    auto* grip = new QGroupBox(tr("Grip hole"));
    m_gripBox = grip;
    auto* gripLayout = new QVBoxLayout(grip);
    auto* gripForm = new QFormLayout;
    m_handle = spin(0.5, 50.0, 0.1, tr(" mm"));
    m_handle->setToolTip(tr("The diameter of the handles the hand holds"));
    gripForm->addRow(tr("Handle diameter:"), m_handle);
    m_gripFit = spin(0.0, 1.0, 0.05, tr(" mm"));
    m_gripFit->setToolTip(tr("Room all round the handle: less grips tighter"));
    gripForm->addRow(tr("Fit:"), m_gripFit);
    gripLayout->addLayout(gripForm);
    m_click = new QComboBox;
    m_click->addItem(tr("The way in"));
    m_click->addItem(tr("The way out"));
    m_click->setToolTip(tr("Click where the handle goes in; to aim it, choose \"The way out\" and click where it "
                           "comes out (turn the view round to see it)"));
    gripLayout->addWidget(labelled(tr("Clicking the hand sets:"), m_click));
    auto* straight = new QPushButton(tr("Straight In"));
    straight->setToolTip(tr("Forgets the way out: the hole goes straight in from the way in, through the hand"));
    gripLayout->addWidget(straight);
    layout->addWidget(grip);

    m_hint = new QLabel;
    m_hint->setWordWrap(true);
    layout->addWidget(m_hint);
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
    m_shape->setCurrentIndex(prefs.value(QStringLiteral("ports/shape"), 0).toInt());
    m_size->setValue(prefs.value(QStringLiteral("ports/size"), 3.0).toDouble());
    m_length->setValue(prefs.value(QStringLiteral("ports/length"), 4.0).toDouble());
    m_end->setCurrentIndex(prefs.value(QStringLiteral("ports/end"), 1).toInt());
    m_ball->setValue(prefs.value(QStringLiteral("ports/ball"), 4.0).toDouble());
    m_printer->setCurrentIndex(prefs.value(QStringLiteral("ports/resin"), true).toBool() ? 0 : 1);
    m_socketFit->setValue(prefs.value(QStringLiteral("ports/socketFit"), 0.1).toDouble());
    m_handle->setValue(prefs.value(QStringLiteral("ports/handle"), 3.0).toDouble());
    m_gripFit->setValue(prefs.value(QStringLiteral("ports/gripFit"), 0.1).toDouble());

    const auto remember = [this] {
        QSettings p = preferences();
        p.setValue(QStringLiteral("ports/shape"), m_shape->currentIndex());
        p.setValue(QStringLiteral("ports/size"), m_size->value());
        p.setValue(QStringLiteral("ports/length"), m_length->value());
        p.setValue(QStringLiteral("ports/end"), m_end->currentIndex());
        p.setValue(QStringLiteral("ports/ball"), m_ball->value());
        p.setValue(QStringLiteral("ports/resin"), resin());
        p.setValue(QStringLiteral("ports/socketFit"), m_socketFit->value());
        p.setValue(QStringLiteral("ports/handle"), m_handle->value());
        p.setValue(QStringLiteral("ports/gripFit"), m_gripFit->value());
    };
    for (QDoubleSpinBox* s : {m_size, m_length, m_ball, m_turn, m_socketFit, m_handle, m_gripFit})
        connect(s, &QDoubleSpinBox::valueChanged, this, [this, remember] {
            remember();
            emit changed();
        });
    for (QComboBox* c : {m_shape, m_end, m_what})
        connect(c, &QComboBox::currentIndexChanged, this, [this, remember] {
            refresh();
            remember();
            emit changed();
        });
    connect(m_printer, &QComboBox::currentIndexChanged, this, [this, remember] {
        // A new printer: its usual fit to start from.
        m_socketFit->setValue(resin() ? 0.1 : 0.2);
        remember();
        emit changed();
    });
    connect(straight, &QPushButton::clicked, this, &PortPanel::straightRequested);
    connect(m_apply, &QPushButton::clicked, this, &PortPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &PortPanel::cancelRequested);
    refresh();
}

void PortPanel::refresh()
{
    const What w = what();
    m_pegBox->setVisible(w != What::Grip);
    m_socketBox->setVisible(w == What::Socket);
    m_gripBox->setVisible(w == What::Grip);
    m_ballRow->setVisible(m_end->currentIndex() == 2);
    m_sizeLabel->setText(m_shape->currentIndex() == 0 ? tr("Diameter:") : tr("Across flats:"));
    m_turn->setEnabled(m_shape->currentIndex() != 0);
    switch (w) {
    case What::Peg:
        m_hint->setText(tr("Click the end of the arm (or the accessory) where the peg goes: it stands straight out of "
                           "the surface there (gold)."));
        break;
    case What::Socket:
        m_hint->setText(tr("Click the hand's wrist where the peg goes in: the socket is cut straight in there (red)."));
        break;
    case What::Grip:
        m_hint->setText(tr("Click the hand where the handle goes in (red). It goes straight in, through the hand, or "
                           "aim it by setting the way out too."));
        break;
    }
    fitHeight();
}

void PortPanel::start(const QString& title, bool editing, What w, const PortFinish& f)
{
    m_title->setText(title);
    {
        const QSignalBlocker block(m_what);
        m_what->setCurrentIndex(int(w));
    }
    m_what->setEnabled(!editing);
    m_standard->setEnabled(true);
    if (editing)
        setFinish(f);
    m_click->setCurrentIndex(0);
    refresh();
    setStatus({}, false);
}

PortPanel::What PortPanel::what() const
{
    return What(m_what->currentIndex());
}

PortFinish PortPanel::finish() const
{
    PortFinish f;
    if (what() == What::Grip) {
        f.kind = PortKind::Grip;
        f.size = m_handle->value();
        f.fit = m_gripFit->value();
        return f;
    }
    f.kind = PortKind::Peg;
    f.shape = PegShape(m_shape->currentIndex());
    f.size = m_size->value();
    f.length = m_length->value();
    f.end = PegEnd(m_end->currentIndex());
    f.ball = m_ball->value();
    f.turn = m_turn->value();
    return f;
}

void PortPanel::setFinish(const PortFinish& f)
{
    const QSignalBlocker a(m_shape), b(m_size), c(m_length), d(m_end), e(m_ball), g(m_turn), h(m_handle), i(m_gripFit);
    if (f.kind == PortKind::Grip) {
        m_handle->setValue(f.size);
        m_gripFit->setValue(f.fit);
    } else {
        m_shape->setCurrentIndex(int(f.shape));
        m_size->setValue(f.size);
        m_length->setValue(f.length);
        m_end->setCurrentIndex(int(f.end));
        m_ball->setValue(f.ball);
        m_turn->setValue(f.turn);
    }
    refresh();
    emit changed();
}

double PortPanel::fit() const
{
    return m_socketFit->value();
}

bool PortPanel::resin() const
{
    return m_printer->currentIndex() == 0;
}

bool PortPanel::clickSetsExit() const
{
    return what() == What::Grip && m_click->currentIndex() == 1;
}

void PortPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #d04020;") : QString());
    m_status->setVisible(!text.isEmpty());
}

void PortPanel::setApplyEnabled(bool on)
{
    m_apply->setEnabled(on);
}

void PortPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    fitHeight();
}

void PortPanel::fitHeight()
{
    // Tall enough for its wrapped words at this width (and what's shown), so the dock
    // scrolls instead of squeezing them.
    layout()->activate();
    const int needed = layout()->hasHeightForWidth() ? layout()->heightForWidth(width()) : sizeHint().height();
    if (needed > 0 && minimumHeight() != needed)
        setMinimumHeight(needed);
}

void PortPanel::attachFormulas(FormulaFields* fields)
{
    fields->add(m_size, QStringLiteral("port.size"), tr("Peg size"));
    fields->add(m_handle, QStringLiteral("port.size"), tr("Handle diameter"));
    fields->add(m_length, QStringLiteral("port.length"), tr("Peg length"));
    fields->add(m_ball, QStringLiteral("port.ball"), tr("Ball diameter"));
    fields->add(m_gripFit, QStringLiteral("port.fit"), tr("Grip fit"));
}

}  // namespace forge::app
