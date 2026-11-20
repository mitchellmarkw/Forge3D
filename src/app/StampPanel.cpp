// SPDX-License-Identifier: GPL-3.0-or-later
#include "StampPanel.h"

#include "forge/core/Stamp.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include <algorithm>

namespace forge::app {
namespace {

QSettings preferences()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"), QStringLiteral("Forge3D"));
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

QDoubleSpinBox* spin(double low, double high, double step, int decimals, const QString& suffix)
{
    auto* box = new QDoubleSpinBox;
    box->setRange(low, high);
    box->setSingleStep(step);
    box->setDecimals(decimals);
    box->setSuffix(suffix);
    box->setKeyboardTracking(false);
    return box;
}

}  // namespace

StampPanel::StampPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    m_title->setWordWrap(true);
    layout->addWidget(m_title);
    auto* hint = new QLabel(tr("Click the shape to stamp one there; drag across it to stamp a row. Stamps stand out "
                               "of the surface or are cut into it."));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_kind = new QComboBox;
    m_kind->setToolTip(tr("The stamp: a built-in one, or one of your custom shapes (saved with Add → Custom Shapes)"));
    layout->addWidget(labelled(tr("Stamp:"), m_kind));
    m_way = new QComboBox;
    m_way->addItem(tr("Raised (stands out)"));
    m_way->addItem(tr("Sunk (cut in)"));
    layout->addWidget(labelled(tr("Raised or sunk:"), m_way));

    auto* form = new QFormLayout;
    m_size = spin(0.2, 100.0, 0.5, 2, tr(" mm"));
    m_size->setToolTip(tr("How wide each stamp is, the widest way"));
    form->addRow(tr("Size:"), m_size);
    m_height = spin(0.05, 100.0, 0.1, 2, tr(" mm"));
    m_height->setToolTip(tr("How far it stands out (or goes in)"));
    form->addRow(tr("Height:"), m_height);
    m_spin = spin(-180.0, 180.0, 15.0, 0, tr("°"));
    m_spin->setWrapping(true);
    m_spin->setToolTip(tr("Turns each stamp on the surface: 0 lies along a row (stitches at 30° look sewn)"));
    form->addRow(tr("Turn:"), m_spin);
    m_spacing = spin(0.1, 1000.0, 0.5, 2, tr(" mm"));
    m_spacing->setToolTip(tr("How far apart, middle to middle, when you drag a row (spread so the last lands "
                             "where you let go)"));
    form->addRow(tr("Row spacing:"), m_spacing);
    layout->addLayout(form);

    m_count = new QLabel;
    layout->addWidget(m_count);
    auto* stampButtons = new QHBoxLayout;
    auto* removeLast = new QPushButton(tr("Remove Last"));
    removeLast->setToolTip(tr("Take away the last stamp or row placed"));
    auto* clear = new QPushButton(tr("Clear"));
    stampButtons->addWidget(removeLast);
    stampButtons->addWidget(clear);
    stampButtons->addStretch(1);
    layout->addLayout(stampButtons);

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

    connect(m_kind, &QComboBox::currentIndexChanged, this, [this] {
        // A new kind starts at its usual height for the size.
        const StampFinish f = settings();
        const QSignalBlocker block(m_height);
        m_height->setValue(stamp::usualHeight(f.kind, f.size));
        remember();
        emit kindChanged();
        emit changed();
    });
    for (QDoubleSpinBox* box : {m_size, m_height, m_spin, m_spacing})
        connect(box, &QDoubleSpinBox::valueChanged, this, [this] {
            remember();
            emit changed();
        });
    connect(m_way, &QComboBox::currentIndexChanged, this, [this] {
        remember();
        emit changed();
    });
    connect(removeLast, &QPushButton::clicked, this, &StampPanel::removeLastRequested);
    connect(clear, &QPushButton::clicked, this, &StampPanel::clearRequested);
    connect(m_apply, &QPushButton::clicked, this, &StampPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &StampPanel::cancelRequested);
}

void StampPanel::start(const QString& title, const QStringList& custom, const StampFinish* editing,
                       const QString& editingCustom)
{
    m_title->setText(title);
    const QSignalBlocker a(m_kind), b(m_size), c(m_height), d(m_way), e(m_spin), f(m_spacing);
    m_kind->clear();
    m_builtIn = stamp::kKindCount - 1;  // all but Custom
    for (int k = 0; k < m_builtIn; ++k)
        m_kind->addItem(tr(stamp::title(StampKind(k))));
    if (editing && editing->kind == StampKind::Custom) {
        m_kind->insertSeparator(m_kind->count());
        m_kind->addItem(editingCustom);
    } else if (!custom.isEmpty()) {
        m_kind->insertSeparator(m_kind->count());
        for (const QString& name : custom)
            m_kind->addItem(tr("%1 (custom shape)").arg(name));
    }
    m_kind->setEnabled(!editing);  // a step keeps its kind of stamp
    if (editing) {
        m_kind->setCurrentIndex(editing->kind == StampKind::Custom ? m_kind->count() - 1 : int(editing->kind));
        m_size->setValue(editing->size);
        m_height->setValue(editing->height);
        m_way->setCurrentIndex(editing->sunk ? 1 : 0);
        m_spin->setValue(editing->spin);
        m_spacing->setValue(editing->spacing);
    } else {
        const QSettings prefs = preferences();
        m_kind->setCurrentIndex(std::clamp(prefs.value(QStringLiteral("stamp/kind"), 0).toInt(), 0, m_builtIn - 1));
        m_size->setValue(prefs.value(QStringLiteral("stamp/size"), 3.0).toDouble());
        m_height->setValue(prefs.value(QStringLiteral("stamp/height"),
                                       stamp::usualHeight(StampKind(m_kind->currentIndex()), m_size->value()))
                               .toDouble());
        m_way->setCurrentIndex(prefs.value(QStringLiteral("stamp/sunk"), false).toBool() ? 1 : 0);
        m_spin->setValue(prefs.value(QStringLiteral("stamp/spin"), 0.0).toDouble());
        m_spacing->setValue(prefs.value(QStringLiteral("stamp/spacing"), 5.0).toDouble());
    }
    setStatus({}, false);
}

StampFinish StampPanel::settings() const
{
    StampFinish f;
    const int i = m_kind->currentIndex();
    f.kind = i >= 0 && i < m_builtIn ? StampKind(i) : StampKind::Custom;
    f.size = m_size->value();
    f.height = m_height->value();
    f.sunk = m_way->currentIndex() == 1;
    f.spin = m_spin->value();
    f.spacing = m_spacing->value();
    return f;
}

int StampPanel::customIndex() const
{
    const int i = m_kind->currentIndex();
    return i > m_builtIn ? i - m_builtIn - 1 : -1;  // past the separator
}

void StampPanel::remember() const
{
    if (!m_kind->isEnabled())
        return;  // changing a step: keep what new stamps start with
    QSettings prefs = preferences();
    if (m_kind->currentIndex() < m_builtIn)
        prefs.setValue(QStringLiteral("stamp/kind"), m_kind->currentIndex());
    prefs.setValue(QStringLiteral("stamp/size"), m_size->value());
    prefs.setValue(QStringLiteral("stamp/height"), m_height->value());
    prefs.setValue(QStringLiteral("stamp/sunk"), m_way->currentIndex() == 1);
    prefs.setValue(QStringLiteral("stamp/spin"), m_spin->value());
    prefs.setValue(QStringLiteral("stamp/spacing"), m_spacing->value());
}

void StampPanel::setCount(int count)
{
    m_count->setText(count == 0 ? tr("No stamps yet.") : count == 1 ? tr("1 stamp.") : tr("%1 stamps.").arg(count));
    m_apply->setEnabled(count > 0);
}

void StampPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #d04020;") : QString());
    m_status->setVisible(!text.isEmpty());
}

}  // namespace forge::app
