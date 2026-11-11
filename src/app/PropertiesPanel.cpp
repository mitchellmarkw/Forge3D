// SPDX-License-Identifier: GPL-3.0-or-later
#include "PropertiesPanel.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <vector>
#include <cmath>
#include <utility>
#include <variant>

namespace forge::app {

namespace {

constexpr double kMinLength = 0.01;    // mm
constexpr double kMaxLength = 10000.0;  // 10 m
constexpr double kMaxOffset = 10000.0;

bool differs(double a, double b)
{
    return std::abs(a - b) > 1e-9;
}

QDoubleSpinBox* makeSpin(double min, double max, const QString& suffix, int decimals, const QString& tip)
{
    auto* spin = new QDoubleSpinBox;
    spin->setRange(min, max);
    spin->setDecimals(decimals);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);  // apply when Enter is pressed or focus leaves, not per keystroke
    spin->setAccelerated(true);
    spin->setToolTip(tip);
    spin->setMinimumWidth(110);
    return spin;
}

}  // namespace

PropertiesPanel::PropertiesPanel(QWidget* parent) : QWidget(parent)
{
    m_pages = new QStackedWidget(this);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->addWidget(m_pages);

    // Page 0: nothing selected
    m_emptyHint = new QLabel(tr("Click a body in the view or in the Objects list to see and edit "
                                "its size and position."));
    m_emptyHint->setWordWrap(true);
    m_emptyHint->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_emptyHint->setStyleSheet(QStringLiteral("color: palette(mid);"));
    // Page 0 holds the hint, plus a note when several shapes are selected.
    auto* emptyPage = new QWidget;
    auto* emptyLayout = new QVBoxLayout(emptyPage);
    emptyLayout->setContentsMargins(0, 0, 0, 0);
    emptyLayout->addWidget(m_emptyHint);
    m_selectionHint = new QLabel;
    m_selectionHint->setWordWrap(true);
    emptyLayout->addWidget(m_selectionHint);
    emptyLayout->addStretch(1);
    m_pages->addWidget(emptyPage);

    // Page 1: a body
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* general = new QFormLayout;
    m_name = new QLineEdit;
    m_name->setPlaceholderText(tr("Name of this body"));
    m_name->setToolTip(tr("Press Enter to rename"));
    m_type = new QLabel;
    general->addRow(tr("Name:"), m_name);
    general->addRow(tr("Shape:"), m_type);
    m_context = new QLabel;
    m_context->setWordWrap(true);
    m_context->setStyleSheet(QStringLiteral("color: palette(mid);"));
    general->addRow(m_context);
    auto* separate = new QPushButton(tr("Separate into parts"));
    separate->setToolTip(tr("Break this combined shape back into its parts, each left where it is"));
    connect(separate, &QPushButton::clicked, this, &PropertiesPanel::separateRequested);
    general->addRow(separate);
    m_separateButton = separate;
    auto* editCut = new QPushButton(tr("Move the cut..."));
    editCut->setToolTip(tr("Change where this half was cut; the shape updates"));
    connect(editCut, &QPushButton::clicked, this, &PropertiesPanel::editCutRequested);
    general->addRow(editCut);
    m_editCutButton = editCut;
    layout->addLayout(general);

    // Size: one sub-page per primitive type
    auto* sizeGroup = new QGroupBox(tr("Size"));
    m_sizeGroup = sizeGroup;
    auto* sizeLayout = new QVBoxLayout(sizeGroup);
    // One form for every primitive: up to five length fields whose labels change
    // with the shape, then the Round checkbox for shapes with radii.
    auto* sizeForm = new QFormLayout;
    sizeForm->setContentsMargins(0, 0, 0, 0);
    m_sizeForm = sizeForm;
    for (std::size_t i = 0; i < m_sizeFields.size(); ++i) {
        m_sizeFields[i] = makeLength(QString());
        m_sizeLabels[i] = new QLabel;
        sizeForm->addRow(m_sizeLabels[i], m_sizeFields[i]);
    }
    m_round = new QCheckBox(tr("Round"));
    sizeForm->addRow(QString(), m_round);
    sizeLayout->addLayout(sizeForm);

    m_uniform = new QCheckBox(tr("Uniform scale (keep proportions)"));
    m_uniform->setToolTip(tr("Changing one size scales all the others by the same amount. "
                             "Also applies to the Resize handles (toolbar: Uniform, key U)."));
    sizeLayout->addWidget(m_uniform);
    layout->addWidget(sizeGroup);

    // Position
    auto* posGroup = new QGroupBox(tr("Position"));
    auto* posForm = new QFormLayout(posGroup);
    m_x = makeSpin(-kMaxOffset, kMaxOffset, tr(" mm"), 2, tr("Where the centre of the base sits, along X"));
    m_y = makeSpin(-kMaxOffset, kMaxOffset, tr(" mm"), 2, tr("Where the centre of the base sits, along Y"));
    m_z = makeSpin(-kMaxOffset, kMaxOffset, tr(" mm"), 2,
                   tr("Height of the base above the plate (before any tilt)"));
    posForm->addRow(tr("X:"), m_x);
    posForm->addRow(tr("Y:"), m_y);
    posForm->addRow(tr("Z (up):"), m_z);
    auto* sit = new QPushButton(tr("Sit on plate"));
    m_sitButton = sit;
    sit->setToolTip(tr("Move the body up or down so its lowest point touches the plate"));
    posForm->addRow(QString(), sit);
    layout->addWidget(posGroup);

    // Rotation
    auto* rotGroup = new QGroupBox(tr("Rotation"));
    auto* rotForm = new QFormLayout(rotGroup);
    const QString order = tr("Turns are applied X first, then Y, then Z, around the body's centre.");
    m_rotX = makeSpin(-360.0, 360.0, tr(" °"), 1, tr("Tip forward or back (around the X axis). ") + order);
    m_rotY = makeSpin(-360.0, 360.0, tr(" °"), 1, tr("Tip left or right (around the Y axis). ") + order);
    m_rotZ = makeSpin(-360.0, 360.0, tr(" °"), 1, tr("Turn on the plate (around the Z axis). ") + order);
    for (QDoubleSpinBox* spin : {m_rotX, m_rotY, m_rotZ})
        spin->setWrapping(true);
    rotForm->addRow(tr("Around X:"), m_rotX);
    rotForm->addRow(tr("Around Y:"), m_rotY);
    rotForm->addRow(tr("Around Z:"), m_rotZ);
    layout->addWidget(rotGroup);
    layout->addStretch(1);
    m_pages->addWidget(page);

    connect(m_name, &QLineEdit::editingFinished, this, &PropertiesPanel::commitName);
    for (QDoubleSpinBox* spin : m_sizeFields)
        connect(spin, &QDoubleSpinBox::valueChanged, this, &PropertiesPanel::commitSpec);
    connect(m_uniform, &QCheckBox::toggled, this, &PropertiesPanel::uniformChanged);
    connect(m_round, &QCheckBox::toggled, this, [this](bool on) {
        updateRadiusYEnabled();
        emit keepRoundChanged(on);
        // Ticking Round on an oval shape makes it round again.
        if (on && m_body && !m_body->isBoolean() && !isRound(m_body->spec))
            emit specRequested(m_body->id, madeRound(m_body->spec));
    });
    for (QDoubleSpinBox* spin : {m_x, m_y, m_z, m_rotX, m_rotY, m_rotZ})
        connect(spin, &QDoubleSpinBox::valueChanged, this, &PropertiesPanel::commitPlacement);
    connect(sit, &QPushButton::clicked, this, [this] {
        if (m_body)
            emit sitOnPlateRequested(m_body->id);
    });

    setBody(nullptr);
}

QDoubleSpinBox* PropertiesPanel::makeLength(const QString& tip)
{
    return makeSpin(kMinLength, kMaxLength, tr(" mm"), 2, tip);
}

void PropertiesPanel::setBody(const Body* body, const QString& context, bool isPart)
{
    if (!body) {
        m_body.reset();
        m_emptyHint->setVisible(context.isEmpty());
        m_selectionHint->setText(context);
        m_selectionHint->setVisible(!context.isEmpty());
        m_pages->setCurrentIndex(0);
        return;
    }
    m_body = *body;
    m_pages->setCurrentIndex(1);

    // Filling the fields must not send edit requests back.
    const QSignalBlocker b0(m_name), b1(m_sizeFields[0]), b2(m_sizeFields[1]), b3(m_sizeFields[2]),
        b4(m_sizeFields[3]), b5(m_sizeFields[4]), b6(m_x), b7(m_y), b8(m_z), b9(m_rotX), b10(m_rotY), b11(m_rotZ);

    if (!m_name->hasFocus() || m_name->text() != QString::fromStdString(body->name))
        m_name->setText(QString::fromStdString(body->name));
    m_type->setText(QString::fromStdString(typeName(*body)));
    m_context->setText(context);
    m_context->setVisible(!context.isEmpty());
    m_separateButton->setVisible(body->isBoolean() && !isPart);
    const bool split = body->isBoolean() && body->boolean->op == BooleanOp::Split;
    m_editCutButton->setVisible(split && !isPart);
    if (auto* button = qobject_cast<QPushButton*>(m_separateButton)) {
        button->setText(split ? tr("Remove the cut") : tr("Separate into parts"));
        button->setToolTip(split ? tr("Give back the whole shape, where it is")
                                 : tr("Break this combined shape back into its parts, each left where it is"));
    }
    m_sizeGroup->setVisible(!body->isBoolean());  // a combined shape is sized through its parts
    m_sitButton->setVisible(!isPart);

    if (!body->isBoolean()) {
        const auto& fields = specFields(body->spec);
        for (std::size_t i = 0; i < m_sizeFields.size(); ++i) {
            const bool used = i < fields.size();
            m_sizeForm->setRowVisible(m_sizeFields[i], used);
            if (!used)
                continue;
            m_sizeLabels[i]->setText(tr(fields[i].label) + QLatin1Char(':'));
            m_sizeFields[i]->setToolTip(tr(fields[i].tip));
            m_sizeFields[i]->setMinimum(fields[i].allowZero ? 0.0 : kMinLength);
            m_sizeFields[i]->setValue(specValue(body->spec, i));
        }
        const bool roundable = hasRoundGroups(body->spec);
        m_sizeForm->setRowVisible(m_round, roundable);
        if (roundable) {
            const bool sphere = std::holds_alternative<SphereSpec>(body->spec);
            m_round->setText(sphere ? tr("Round (X, Y and Z radii equal)") : tr("Round (X radius = Y radius)"));
            m_round->setToolTip(sphere ? tr("Untick to give X, Y and Z different radii, making an egg or "
                                            "squashed ball. Ticking it makes it round again, using Radius X.")
                                       : tr("Untick to give X and Y different radii, making an oval shape. "
                                            "Ticking it makes it round again, using the X radii."));
        }
        updateRadiusYEnabled();
    }

    m_x->setValue(body->placement.x);
    m_y->setValue(body->placement.y);
    m_z->setValue(body->placement.z);
    m_rotX->setValue(body->placement.rotationX);
    m_rotY->setValue(body->placement.rotationY);
    m_rotZ->setValue(body->placement.rotationZ);
}

void PropertiesPanel::commitName()
{
    if (!m_body)
        return;
    const QString name = m_name->text().trimmed();
    if (name.isEmpty()) {
        m_name->setText(QString::fromStdString(m_body->name));  // empty names are not allowed
        return;
    }
    if (name.toStdString() != m_body->name)
        emit renameRequested(m_body->id, name);
}

void PropertiesPanel::setUniform(bool on)
{
    const QSignalBlocker block(m_uniform);
    m_uniform->setChecked(on);
}

bool PropertiesPanel::uniform() const
{
    return m_uniform->isChecked();
}

void PropertiesPanel::setKeepRound(bool on)
{
    const QSignalBlocker block(m_round);
    m_round->setChecked(on);
    updateRadiusYEnabled();
}

bool PropertiesPanel::keepRound() const
{
    return m_round->isChecked();
}

void PropertiesPanel::commitSpec()
{
    if (!m_body)
        return;
    // Which field did the user change? Its new value decides the result.
    PrimitiveSpec spec = m_body->spec;
    const bool uniform = m_uniform->isChecked();
    bool changed = false;
    const std::size_t count = specFields(spec).size();
    for (std::size_t i = 0; i < count && i < m_sizeFields.size(); ++i) {
        const double old = specValue(spec, i), v = m_sizeFields[i]->value();
        if (!differs(old, v))
            continue;
        if (uniform && old > 0.0) {
            spec = scaled(spec, v / old);
        } else {
            setSpecValue(spec, i, v);
            // While Round is ticked, a radius sets the other radii of its group too.
            const auto& fields = specFields(spec);
            if (fields[i].roundGroup != 0) {
                for (std::size_t j = 0; j < fields.size(); ++j) {
                    if (j == i || fields[j].roundGroup != fields[i].roundGroup)
                        continue;
                    // A cone end is a point (both radii 0) or an ellipse (both above 0):
                    // going from one to the other moves the partner radius along too.
                    const bool pointChange = fields[i].allowZero && (v == 0.0 || specValue(spec, j) == 0.0);
                    if (m_round->isChecked() || pointChange)
                        setSpecValue(spec, j, v);
                }
            }
        }
        changed = true;
        break;  // one field changes at a time
    }
    if (changed)
        emit specRequested(m_body->id, spec);
}

void PropertiesPanel::updateRadiusYEnabled()
{
    // While Round is ticked, only the first radius of each group can be typed in;
    // the others follow it.
    std::vector<int> seen;
    const auto* fields = m_body ? &specFields(m_body->spec) : nullptr;
    for (std::size_t i = 0; i < m_sizeFields.size(); ++i) {
        bool follower = false;
        if (fields && i < fields->size() && (*fields)[i].roundGroup != 0) {
            const int g = (*fields)[i].roundGroup;
            follower = std::find(seen.begin(), seen.end(), g) != seen.end();
            seen.push_back(g);
        }
        m_sizeFields[i]->setEnabled(!(follower && m_round->isChecked()));
    }
}

void PropertiesPanel::commitPlacement()
{
    if (!m_body)
        return;
    Placement p;
    p.x = m_x->value();
    p.y = m_y->value();
    p.z = m_z->value();
    p.rotationX = m_rotX->value();
    p.rotationY = m_rotY->value();
    p.rotationZ = m_rotZ->value();
    if (!(p == m_body->placement))
        emit placementRequested(m_body->id, p);
}

}  // namespace forge::app
