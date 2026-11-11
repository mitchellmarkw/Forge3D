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

#include <array>
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
    m_pages->addWidget(m_emptyHint);

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
    layout->addLayout(general);

    // Size: one sub-page per primitive type
    auto* sizeGroup = new QGroupBox(tr("Size"));
    auto* sizeLayout = new QVBoxLayout(sizeGroup);
    m_sizePages = new QStackedWidget;
    sizeLayout->addWidget(m_sizePages);

    auto* boxPage = new QWidget;
    auto* boxForm = new QFormLayout(boxPage);
    boxForm->setContentsMargins(0, 0, 0, 0);
    m_boxWidth = makeLength(tr("Size along X"));
    m_boxDepth = makeLength(tr("Size along Y"));
    m_boxHeight = makeLength(tr("Size along Z (up)"));
    boxForm->addRow(tr("Width (X):"), m_boxWidth);
    boxForm->addRow(tr("Depth (Y):"), m_boxDepth);
    boxForm->addRow(tr("Height (Z):"), m_boxHeight);
    m_sizePages->addWidget(boxPage);

    auto* cylPage = new QWidget;
    auto* cylForm = new QFormLayout(cylPage);
    cylForm->setContentsMargins(0, 0, 0, 0);
    m_cylRadiusX = makeLength(tr("Distance from the centre line to the side, along X"));
    m_cylRadiusY = makeLength(tr("Distance from the centre line to the side, along Y"));
    m_cylHeight = makeLength(tr("Size along Z (up)"));
    m_round = new QCheckBox(tr("Round (radius X = radius Y)"));
    m_round->setToolTip(tr("Untick to give X and Y different radii, making an elliptical cylinder. "
                           "Ticking it makes the cylinder round again, using Radius X."));
    m_round->setChecked(true);
    cylForm->addRow(tr("Radius X:"), m_cylRadiusX);
    cylForm->addRow(tr("Radius Y:"), m_cylRadiusY);
    cylForm->addRow(QString(), m_round);
    cylForm->addRow(tr("Height (Z):"), m_cylHeight);
    m_sizePages->addWidget(cylPage);

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
    for (QDoubleSpinBox* spin : {m_boxWidth, m_boxDepth, m_boxHeight, m_cylRadiusX, m_cylRadiusY, m_cylHeight})
        connect(spin, &QDoubleSpinBox::valueChanged, this, &PropertiesPanel::commitSpec);
    connect(m_uniform, &QCheckBox::toggled, this, &PropertiesPanel::uniformChanged);
    connect(m_round, &QCheckBox::toggled, this, [this](bool on) {
        m_cylRadiusY->setEnabled(!on);
        emit keepRoundChanged(on);
        // Ticking Round on an elliptical cylinder makes it round again.
        if (on && m_body) {
            if (const auto* cyl = std::get_if<CylinderSpec>(&m_body->spec); cyl && !cyl->isRound())
                emit specRequested(m_body->id, CylinderSpec{cyl->radiusX, cyl->height});
        }
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

void PropertiesPanel::setBody(const Body* body)
{
    if (!body) {
        m_body.reset();
        m_pages->setCurrentIndex(0);
        return;
    }
    m_body = *body;
    m_pages->setCurrentIndex(1);

    // Filling the fields must not send edit requests back.
    const QSignalBlocker b0(m_name), b1(m_boxWidth), b2(m_boxDepth), b3(m_boxHeight), b4(m_cylRadiusX),
        b5(m_cylHeight), b6(m_x), b7(m_y), b8(m_z), b9(m_rotX), b10(m_rotY), b11(m_rotZ), b12(m_cylRadiusY);

    if (!m_name->hasFocus() || m_name->text() != QString::fromStdString(body->name))
        m_name->setText(QString::fromStdString(body->name));
    m_type->setText(QString::fromStdString(typeName(body->spec)));

    // Size the stack to the page being shown; a stacked widget otherwise reserves
    // room for its tallest page (the cylinder's), leaving a gap under a box's fields.
    const int sizePage = std::holds_alternative<BoxSpec>(body->spec) ? 0 : 1;
    for (int i = 0; i < m_sizePages->count(); ++i)
        m_sizePages->widget(i)->setSizePolicy(QSizePolicy::Preferred,
                                              i == sizePage ? QSizePolicy::Preferred : QSizePolicy::Ignored);
    m_sizePages->setCurrentIndex(sizePage);
    m_sizePages->adjustSize();

    if (const auto* box = std::get_if<BoxSpec>(&body->spec)) {
        m_boxWidth->setValue(box->width);
        m_boxDepth->setValue(box->depth);
        m_boxHeight->setValue(box->height);
    } else {
        const auto& cyl = std::get<CylinderSpec>(body->spec);
        m_cylRadiusX->setValue(cyl.radiusX);
        m_cylRadiusY->setValue(cyl.radiusY);
        m_cylHeight->setValue(cyl.height);
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
    m_cylRadiusY->setEnabled(!on);
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
    // Scale everything by `factor`, or set one value.
    const auto apply = [uniform](double& field, double newValue, auto& all) {
        if (uniform && field > 0.0) {
            const double factor = newValue / field;
            for (double* v : all)
                *v *= factor;
        } else {
            field = newValue;
        }
    };

    bool changed = false;
    if (auto* box = std::get_if<BoxSpec>(&spec)) {
        std::array<double*, 3> all{&box->width, &box->depth, &box->height};
        const std::array<std::pair<double*, QDoubleSpinBox*>, 3> fields{
            {{&box->width, m_boxWidth}, {&box->depth, m_boxDepth}, {&box->height, m_boxHeight}}};
        for (const auto& [field, spin] : fields) {
            if (differs(*field, spin->value())) {
                apply(*field, spin->value(), all);
                changed = true;
                break;  // one field changes at a time
            }
        }
    } else {
        auto& cyl = std::get<CylinderSpec>(spec);
        std::array<double*, 3> all{&cyl.radiusX, &cyl.radiusY, &cyl.height};
        if (differs(cyl.radiusX, m_cylRadiusX->value())) {
            const double v = m_cylRadiusX->value();
            apply(cyl.radiusX, v, all);
            if (!uniform && m_round->isChecked())
                cyl.radiusY = v;
            changed = true;
        } else if (differs(cyl.radiusY, m_cylRadiusY->value())) {
            apply(cyl.radiusY, m_cylRadiusY->value(), all);
            changed = true;
        } else if (differs(cyl.height, m_cylHeight->value())) {
            apply(cyl.height, m_cylHeight->value(), all);
            changed = true;
        }
    }
    if (changed)
        emit specRequested(m_body->id, spec);
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
