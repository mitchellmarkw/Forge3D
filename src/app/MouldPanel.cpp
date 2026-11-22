// SPDX-License-Identifier: GPL-3.0-or-later
#include "MouldPanel.h"

#include <QCheckBox>
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

#include <algorithm>
#include <cmath>

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

// The way a cut goes, by the axis it's square to.
QString directionName(int axis)
{
    switch (axis) {
    case 0: return QObject::tr("Vertical (left and right)");
    case 1: return QObject::tr("Vertical (front and back)");
    default: return QObject::tr("Horizontal (top and bottom)");
    }
}

QString sideWord(int axis, bool positive)
{
    switch (axis) {
    case 0: return positive ? QObject::tr("right") : QObject::tr("left");
    case 1: return positive ? QObject::tr("back") : QObject::tr("front");
    default: return positive ? QObject::tr("top") : QObject::tr("bottom");
    }
}

QString placeLabel(int axis)
{
    switch (axis) {
    case 0: return QObject::tr("Cut at (X):");
    case 1: return QObject::tr("Cut at (Y):");
    default: return QObject::tr("Cut at (height):");
    }
}

QLabel* wrapped(const QString& text)
{
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    return label;
}

}  // namespace

MouldPanel::MouldPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    m_title->setWordWrap(true);
    layout->addWidget(m_title);
    layout->addWidget(wrapped(tr("A mould box round the part, for casting resin copies of it: print the pieces, put "
                                 "them together and pour into the spout.")));

    auto* split = new QGroupBox(tr("Split"));
    auto* splitLayout = new QVBoxLayout(split);
    m_direction = new QComboBox;
    for (int axis : {2, 0, 1})
        m_direction->addItem(directionName(axis), axis);
    m_direction->setToolTip(tr("Which way the mould comes apart: horizontal makes top and bottom halves pulled up and "
                               "down; vertical makes halves pulled to the sides"));
    splitLayout->addWidget(labelled(tr("Split direction:"), m_direction));
    m_parting = millimetres(-10000.0, 10000.0, 0.5);
    m_parting->setToolTip(tr("Where the flat face is that the halves meet at (shown yellow). It must cross the part; "
                             "where the part is widest opens most easily"));
    auto* widest = new QPushButton(tr("Where It's Widest"));
    widest->setToolTip(tr("Puts the cut where the part's outline is biggest"));
    auto* partingRow = new QHBoxLayout;
    partingRow->setContentsMargins(0, 0, 0, 0);
    partingRow->addWidget(m_parting, 1);
    partingRow->addWidget(widest);
    m_partingLabel = new QLabel(placeLabel(2));
    splitLayout->addWidget(m_partingLabel);
    splitLayout->addLayout(partingRow);
    m_undercut = new QLabel;
    m_undercut->setWordWrap(true);
    m_undercut->setToolTip(tr("Where a piece pulled straight off would catch on the part (shown red). A rigid "
                              "printed mould can't come off there; a soft silicone one may. Another direction or "
                              "place, or more pieces, can free it"));
    splitLayout->addWidget(m_undercut);
    m_best = new QPushButton(tr("Find Best Split"));
    m_best->setToolTip(tr("Tries each direction at several places and picks the one the pieces catch on least (of "
                          "those, the widest)"));
    splitLayout->addWidget(m_best);
    m_pieces = new QComboBox;
    m_pieces->addItem(tr("2 (one cut)"), 2);
    m_pieces->addItem(tr("3 (one half cut again)"), 3);
    m_pieces->addItem(tr("4 (both halves cut again)"), 4);
    m_pieces->setToolTip(tr("More pieces free a part with undercuts: each piece can come away along either of its "
                            "cuts"));
    splitLayout->addWidget(labelled(tr("Pieces:"), m_pieces));
    m_secondBox = new QWidget;
    auto* second = new QVBoxLayout(m_secondBox);
    second->setContentsMargins(0, 0, 0, 0);
    m_direction2 = new QComboBox;
    m_direction2->setToolTip(tr("Which way the second cut goes"));
    second->addWidget(labelled(tr("Second cut direction:"), m_direction2));
    m_secondIn = new QComboBox;
    m_secondIn->setToolTip(tr("Which half the second cut goes through"));
    m_secondInRow = labelled(tr("Second cut goes through:"), m_secondIn);
    second->addWidget(m_secondInRow);
    auto* secondForm = new QFormLayout;
    m_parting2 = millimetres(-10000.0, 10000.0, 0.5);
    m_parting2->setToolTip(tr("Where the second cut is (shown yellow too)"));
    auto* middle = new QPushButton(tr("Middle"));
    middle->setToolTip(tr("Puts the second cut through the middle of the part"));
    auto* parting2Row = new QHBoxLayout;
    parting2Row->setContentsMargins(0, 0, 0, 0);
    parting2Row->addWidget(m_parting2, 1);
    parting2Row->addWidget(middle);
    secondForm->addRow(tr("Second cut at:"), parting2Row);
    second->addLayout(secondForm);
    splitLayout->addWidget(m_secondBox);
    layout->addWidget(split);

    auto* box = new QGroupBox(tr("Box"));
    auto* boxForm = new QFormLayout(box);
    m_wall = millimetres(2.0, 50.0, 0.5);
    m_wall->setToolTip(tr("How much mould there is round the part on every side"));
    boxForm->addRow(tr("Wall thickness:"), m_wall);
    layout->addWidget(box);

    auto* keysBox = new QGroupBox(tr("Registration keys"));
    auto* keysLayout = new QVBoxLayout(keysBox);
    m_keys = new QCheckBox(tr("Keys in the corners"));
    m_keys->setToolTip(tr("Pegs on one piece fit holes in the next, at each cut, so the pieces line up"));
    keysLayout->addWidget(m_keys);
    auto* keysForm = new QFormLayout;
    m_keySize = millimetres(1.0, 30.0, 0.5);
    m_keySize->setToolTip(tr("The pegs' diameter where they meet the parting face (smaller if the walls are thin)"));
    keysForm->addRow(tr("Key size:"), m_keySize);
    m_keyFit = millimetres(0.0, 1.0, 0.05);
    m_keyFit->setToolTip(tr("How much bigger the holes are than the pegs, all round"));
    keysForm->addRow(tr("Key fit:"), m_keyFit);
    keysLayout->addLayout(keysForm);
    layout->addWidget(keysBox);

    auto* pourBox = new QGroupBox(tr("Spout and air vents"));
    auto* pour = new QVBoxLayout(pourBox);
    auto* pourForm = new QFormLayout;
    m_spoutSize = millimetres(1.0, 30.0, 0.5);
    m_spoutSize->setToolTip(tr("The spout (red, with a funnel) runs from the part's top up out of the mould. Its "
                               "diameter: 5 to 8 mm pours well"));
    pourForm->addRow(tr("Spout size:"), m_spoutSize);
    m_ventSize = millimetres(0.5, 10.0, 0.25);
    m_ventSize->setToolTip(tr("Air vents (thin red rods) let out air trapped at the part's other high spots. Their "
                              "diameter: 1.5 to 2.5 mm"));
    pourForm->addRow(tr("Vent size:"), m_ventSize);
    pour->addLayout(pourForm);
    m_click = new QComboBox;
    m_click->addItem(tr("An air vent"));
    m_click->addItem(tr("The spout"));
    m_click->setToolTip(tr("What clicking the part puts there (with a horizontal split, above the parting face)"));
    pour->addWidget(labelled(tr("Clicking the part places:"), m_click));
    m_vents = new QLabel;
    m_vents->setWordWrap(true);
    pour->addWidget(m_vents);
    auto* ventButtons = new QHBoxLayout;
    auto* place = new QPushButton(tr("Place Vents"));
    place->setToolTip(tr("Air vents at the part's high spots, where air would be trapped (instead of the ones "
                         "there)"));
    auto* removeLast = new QPushButton(tr("Remove Last"));
    auto* clear = new QPushButton(tr("Clear"));
    ventButtons->addWidget(place);
    ventButtons->addWidget(removeLast);
    ventButtons->addWidget(clear);
    pour->addLayout(ventButtons);
    layout->addWidget(pourBox);

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
    m_wall->setValue(prefs.value(QStringLiteral("mould/wall"), 8.0).toDouble());
    m_keys->setChecked(prefs.value(QStringLiteral("mould/keys"), true).toBool());
    m_keySize->setValue(prefs.value(QStringLiteral("mould/keySize"), 6.0).toDouble());
    m_keyFit->setValue(prefs.value(QStringLiteral("mould/keyFit"), 0.2).toDouble());
    m_spoutSize->setValue(prefs.value(QStringLiteral("mould/spoutSize"), 6.0).toDouble());
    m_ventSize->setValue(prefs.value(QStringLiteral("mould/ventSize"), 2.0).toDouble());

    const auto remember = [this] {
        QSettings p = preferences();
        p.setValue(QStringLiteral("mould/wall"), m_wall->value());
        p.setValue(QStringLiteral("mould/keys"), m_keys->isChecked());
        p.setValue(QStringLiteral("mould/keySize"), m_keySize->value());
        p.setValue(QStringLiteral("mould/keyFit"), m_keyFit->value());
        p.setValue(QStringLiteral("mould/spoutSize"), m_spoutSize->value());
        p.setValue(QStringLiteral("mould/ventSize"), m_ventSize->value());
    };
    for (QDoubleSpinBox* spin : {m_wall, m_keySize, m_keyFit, m_spoutSize, m_ventSize})
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this, remember] {
            remember();
            emit changed();
        });
    connect(m_parting, &QDoubleSpinBox::valueChanged, this, &MouldPanel::changed);
    connect(m_parting2, &QDoubleSpinBox::valueChanged, this, &MouldPanel::changed);
    connect(m_direction, &QComboBox::currentIndexChanged, this, [this] {
        refreshCuts();
        emit directionChanged();
    });
    connect(m_pieces, &QComboBox::currentIndexChanged, this, [this] {
        refreshCuts();
        emit changed();
    });
    connect(m_direction2, &QComboBox::currentIndexChanged, this, [this] {
        refreshCuts();
        const std::size_t k = std::size_t(axis2());
        m_parting2->setValue(std::round((m_part.min[k] + m_part.max[k]) / 2.0 * 10.0) / 10.0);  // tells the window
        emit changed();
    });
    connect(m_secondIn, &QComboBox::currentIndexChanged, this, &MouldPanel::changed);
    connect(middle, &QPushButton::clicked, this, [this] {
        const std::size_t k = std::size_t(axis2());
        m_parting2->setValue(std::round((m_part.min[k] + m_part.max[k]) / 2.0 * 10.0) / 10.0);
    });
    connect(m_best, &QPushButton::clicked, this, &MouldPanel::bestRequested);
    connect(m_keys, &QCheckBox::toggled, this, [this, remember](bool on) {
        m_keySize->setEnabled(on);
        m_keyFit->setEnabled(on);
        remember();
        emit changed();
    });
    connect(widest, &QPushButton::clicked, this, &MouldPanel::widestRequested);
    connect(place, &QPushButton::clicked, this, &MouldPanel::placeVentsRequested);
    connect(removeLast, &QPushButton::clicked, this, &MouldPanel::removeLastRequested);
    connect(clear, &QPushButton::clicked, this, &MouldPanel::clearRequested);
    connect(m_apply, &QPushButton::clicked, this, &MouldPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &MouldPanel::cancelRequested);
}

void MouldPanel::start(const QString& title, bool editing, const MouldFinish& f, const Bounds& part)
{
    m_title->setText(title);
    m_part = part;
    const QSignalBlocker a(m_parting), b(m_wall), c(m_keys), d(m_keySize), e(m_keyFit), g(m_spoutSize), h(m_ventSize);
    const QSignalBlocker i(m_direction), j(m_pieces), k(m_direction2), l(m_secondIn), m(m_parting2);
    m_direction->setCurrentIndex(std::max(0, m_direction->findData(f.axis)));
    m_pieces->setCurrentIndex(!f.second ? 0 : f.secondIn == 0 ? 2 : 1);
    refreshCuts();
    m_direction2->setCurrentIndex(std::max(0, m_direction2->findData(f.axis2)));
    refreshCuts();
    m_secondIn->setCurrentIndex(f.secondIn < 0 ? 1 : 0);
    setRange(m_parting, f.axis);
    m_parting->setValue(f.parting);
    const std::size_t k2 = std::size_t(axis2());
    m_parting2->setValue(f.second ? f.parting2 : std::round((part.min[k2] + part.max[k2]) / 2.0 * 10.0) / 10.0);
    // The pieces and the way the cuts go are fixed once made.
    for (QWidget* w : {static_cast<QWidget*>(m_direction), static_cast<QWidget*>(m_pieces),
                       static_cast<QWidget*>(m_direction2), static_cast<QWidget*>(m_secondIn),
                       static_cast<QWidget*>(m_best)})
        w->setEnabled(!editing);
    if (editing) {
        m_wall->setValue(f.wall);
        m_keys->setChecked(f.keys);
        m_keySize->setValue(f.keySize);
        m_keyFit->setValue(f.keyFit);
        m_spoutSize->setValue(f.spoutSize);
        m_ventSize->setValue(f.ventSize);
    }
    m_keySize->setEnabled(m_keys->isChecked());
    m_keyFit->setEnabled(m_keys->isChecked());
    m_click->setCurrentIndex(0);
    setStatus({}, false);
}

int MouldPanel::axis() const
{
    return m_direction->currentData().toInt();
}

int MouldPanel::axis2() const
{
    const QVariant d = m_direction2->currentData();
    return d.isValid() ? d.toInt() : (axis() == 0 ? 1 : 0);
}

void MouldPanel::setRange(QDoubleSpinBox* spin, int axis)
{
    const std::size_t k = std::size_t(axis);
    const QSignalBlocker block(spin);
    spin->setRange(double(m_part.min[k]) + 0.1, double(m_part.max[k]) - 0.1);
}

void MouldPanel::refreshCuts()
{
    const int a = axis();
    m_partingLabel->setText(placeLabel(a));
    setRange(m_parting, a);
    const int pieces = m_pieces->currentData().toInt();
    m_secondBox->setVisible(pieces > 2);
    m_secondInRow->setVisible(pieces == 3);
    {
        // The second cut goes one of the other two ways.
        const QSignalBlocker block(m_direction2);
        const int was = m_direction2->currentData().isValid() ? m_direction2->currentData().toInt() : -1;
        m_direction2->clear();
        for (int other : {2, 0, 1})
            if (other != a)
                m_direction2->addItem(directionName(other), other);
        m_direction2->setCurrentIndex(std::max(0, m_direction2->findData(was)));
    }
    {
        const QSignalBlocker block(m_secondIn);
        const int was = m_secondIn->currentIndex();
        m_secondIn->clear();
        m_secondIn->addItem(tr("The %1 half").arg(sideWord(a, true)));
        m_secondIn->addItem(tr("The %1 half").arg(sideWord(a, false)));
        m_secondIn->setCurrentIndex(std::max(0, was));
    }
    setRange(m_parting2, axis2());
}

void MouldPanel::setSplit(int axis, double at)
{
    {
        const QSignalBlocker block(m_direction);
        m_direction->setCurrentIndex(std::max(0, m_direction->findData(axis)));
        refreshCuts();
    }
    m_parting->setValue(at);  // tells the window
    emit changed();
}

void MouldPanel::setUndercut(double area, double total)
{
    const double share = total > 0 ? area / total : 0.0;
    if (share < 0.001) {
        m_undercut->setText(tr("No undercuts: the pieces come straight off the part."));
        m_undercut->setStyleSheet(QString());
    } else {
        m_undercut->setText(tr("Undercuts (red): %1 mm², %2% of the surface. Try Find Best Split or more pieces.")
                                .arg(area, 0, 'f', 0)
                                .arg(share * 100.0, 0, 'f', share < 0.1 ? 1 : 0));
        m_undercut->setStyleSheet(QStringLiteral("color: #d04020;"));
    }
}

MouldFinish MouldPanel::settings() const
{
    MouldFinish f;
    f.axis = axis();
    f.parting = m_parting->value();
    const int pieces = m_pieces->currentData().toInt();
    f.second = pieces > 2;
    f.axis2 = axis2();
    f.parting2 = m_parting2->value();
    f.secondIn = pieces == 3 ? (m_secondIn->currentIndex() == 0 ? 1 : -1) : 0;
    f.wall = m_wall->value();
    f.keys = m_keys->isChecked();
    f.keySize = m_keySize->value();
    f.keyFit = m_keyFit->value();
    f.spoutSize = m_spoutSize->value();
    f.ventSize = m_ventSize->value();
    return f;
}

void MouldPanel::setParting(double z)
{
    m_parting->setValue(z);  // tells the window (changed)
}

bool MouldPanel::clickSetsSpout() const
{
    return m_click->currentIndex() == 1;
}

void MouldPanel::setVentCount(int count)
{
    m_vents->setText(count == 0   ? tr("No air vents: fine if the part has only one high spot.")
                     : count == 1 ? tr("1 air vent.")
                                  : tr("%1 air vents.").arg(count));
}

void MouldPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #d04020;") : QString());
    m_status->setVisible(!text.isEmpty());
}

void MouldPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // Tall enough for its wrapped words at this width, so the dock scrolls instead of
    // squeezing them on top of each other.
    const int needed = layout()->hasHeightForWidth() ? layout()->heightForWidth(width()) : sizeHint().height();
    if (needed > 0 && minimumHeight() != needed)
        setMinimumHeight(needed);
}

void MouldPanel::setApplyEnabled(bool on)
{
    m_apply->setEnabled(on);
}

}  // namespace forge::app
