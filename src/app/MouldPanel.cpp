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
#include <QSettings>
#include <QVBoxLayout>

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
    layout->addWidget(wrapped(tr("A two-part mould box round the part, for casting resin copies of it: print the two "
                                 "halves, put them together and pour into the spout. The halves part level, so lay the "
                                 "part the way it should open first.")));

    auto* box = new QGroupBox(tr("Box"));
    auto* boxForm = new QFormLayout(box);
    m_parting = millimetres(-10000.0, 10000.0, 0.5);
    m_parting->setToolTip(tr("How high the flat face is where the two halves meet (shown yellow). It must cross the "
                             "part; where the part is widest opens most easily"));
    auto* widest = new QPushButton(tr("Where It's Widest"));
    widest->setToolTip(tr("Parts the mould where the part's outline is biggest"));
    auto* partingRow = new QHBoxLayout;
    partingRow->setContentsMargins(0, 0, 0, 0);
    partingRow->addWidget(m_parting, 1);
    partingRow->addWidget(widest);
    boxForm->addRow(tr("Parting height:"), partingRow);
    m_wall = millimetres(2.0, 50.0, 0.5);
    m_wall->setToolTip(tr("How much mould there is round the part on every side"));
    boxForm->addRow(tr("Wall thickness:"), m_wall);
    layout->addWidget(box);

    auto* keysBox = new QGroupBox(tr("Registration keys"));
    auto* keysLayout = new QVBoxLayout(keysBox);
    m_keys = new QCheckBox(tr("Keys in the corners"));
    m_keys->setToolTip(tr("Pegs on the bottom half fit holes in the top half, so the halves line up"));
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
    pour->addWidget(wrapped(tr("The spout (red, with a funnel) goes from the part's top up out of the mould; air "
                               "vents (thin red rods) let out air trapped at the other high spots.")));
    auto* pourForm = new QFormLayout;
    m_spoutSize = millimetres(1.0, 30.0, 0.5);
    m_spoutSize->setToolTip(tr("The spout's diameter: 5 to 8 mm pours well"));
    pourForm->addRow(tr("Spout size:"), m_spoutSize);
    m_ventSize = millimetres(0.5, 10.0, 0.25);
    m_ventSize->setToolTip(tr("The air vents' diameter: 1.5 to 2.5 mm"));
    pourForm->addRow(tr("Vent size:"), m_ventSize);
    pour->addLayout(pourForm);
    m_click = new QComboBox;
    m_click->addItem(tr("An air vent"));
    m_click->addItem(tr("The spout"));
    m_click->setToolTip(tr("What clicking the part (above the parting face) puts there"));
    pour->addWidget(labelled(tr("Clicking the part places:"), m_click));
    m_vents = new QLabel;
    pour->addWidget(m_vents);
    auto* ventButtons = new QHBoxLayout;
    auto* place = new QPushButton(tr("Place Vents for Me"));
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

void MouldPanel::start(const QString& title, bool editing, const MouldFinish& f, double low, double high)
{
    m_title->setText(title);
    const QSignalBlocker a(m_parting), b(m_wall), c(m_keys), d(m_keySize), e(m_keyFit), g(m_spoutSize), h(m_ventSize);
    m_parting->setRange(low, high);
    m_parting->setValue(f.parting);
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

MouldFinish MouldPanel::settings() const
{
    MouldFinish f;
    f.parting = m_parting->value();
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

void MouldPanel::setApplyEnabled(bool on)
{
    m_apply->setEnabled(on);
}

}  // namespace forge::app
