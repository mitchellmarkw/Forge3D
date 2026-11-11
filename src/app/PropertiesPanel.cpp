// SPDX-License-Identifier: GPL-3.0-or-later
#include "PropertiesPanel.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <cmath>
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
    m_cylRadius = makeLength(tr("Distance from the centre to the side"));
    m_cylHeight = makeLength(tr("Size along Z (up)"));
    cylForm->addRow(tr("Radius:"), m_cylRadius);
    cylForm->addRow(tr("Height (Z):"), m_cylHeight);
    m_sizePages->addWidget(cylPage);
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
    for (QDoubleSpinBox* spin : {m_boxWidth, m_boxDepth, m_boxHeight, m_cylRadius, m_cylHeight})
        connect(spin, &QDoubleSpinBox::valueChanged, this, &PropertiesPanel::commitSpec);
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
    const QSignalBlocker b0(m_name), b1(m_boxWidth), b2(m_boxDepth), b3(m_boxHeight), b4(m_cylRadius),
        b5(m_cylHeight), b6(m_x), b7(m_y), b8(m_z), b9(m_rotX), b10(m_rotY), b11(m_rotZ);

    if (!m_name->hasFocus() || m_name->text() != QString::fromStdString(body->name))
        m_name->setText(QString::fromStdString(body->name));
    m_type->setText(QString::fromStdString(typeName(body->spec)));

    if (const auto* box = std::get_if<BoxSpec>(&body->spec)) {
        m_sizePages->setCurrentIndex(0);
        m_boxWidth->setValue(box->width);
        m_boxDepth->setValue(box->depth);
        m_boxHeight->setValue(box->height);
    } else {
        const auto& cyl = std::get<CylinderSpec>(body->spec);
        m_sizePages->setCurrentIndex(1);
        m_cylRadius->setValue(cyl.radius);
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

void PropertiesPanel::commitSpec()
{
    if (!m_body)
        return;
    PrimitiveSpec spec = m_body->spec;
    bool changed = false;
    if (auto* box = std::get_if<BoxSpec>(&spec)) {
        changed = differs(box->width, m_boxWidth->value()) || differs(box->depth, m_boxDepth->value()) ||
                  differs(box->height, m_boxHeight->value());
        *box = BoxSpec{m_boxWidth->value(), m_boxDepth->value(), m_boxHeight->value()};
    } else {
        auto& cyl = std::get<CylinderSpec>(spec);
        changed = differs(cyl.radius, m_cylRadius->value()) || differs(cyl.height, m_cylHeight->value());
        cyl = CylinderSpec{m_cylRadius->value(), m_cylHeight->value()};
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
