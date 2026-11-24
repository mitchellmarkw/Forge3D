// SPDX-License-Identifier: GPL-3.0-or-later
#include "PropertiesPanel.h"

#include "forge/core/Variables.h"

#include "WordsEditor.h"
#include "forge/core/Text.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QContextMenuEvent>
#include <QMenu>
#include <QKeyEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
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

// Shows or hides a row of a form. Qt 6.4's QFormLayout::setRowVisible writes past the end
// of its own memory when the form is next measured (QTBUG-109237), which crashed the app
// some time later; there the row's widgets are hidden instead.
void showRow(QFormLayout* form, QWidget* field, bool on)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    form->setRowVisible(field, on);
#else
    field->setVisible(on);
    if (QWidget* label = form->labelForField(field))
        label->setVisible(on);
#endif
}

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
    // Only the page shown counts towards the panel's size, so the panel (which scrolls
    // when tall) isn't as tall as the biggest page while it shows a one-line hint.
    connect(m_pages, &QStackedWidget::currentChanged, this, [this](int current) {
        for (int i = 0; i < m_pages->count(); ++i)
            m_pages->widget(i)->setSizePolicy(QSizePolicy::Preferred,
                                              i == current ? QSizePolicy::Preferred : QSizePolicy::Ignored);
        m_pages->adjustSize();
        updateGeometry();
    });
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
    // Below the form, not in it: in Qt 6.4 a form holding a wrapping label and hidden
    // rows writes past the end of its own memory when it's measured, which crashed the app
    // some time later (after sculpting, as the panel changed).
    layout->addLayout(general);
    layout->addWidget(m_context);
    general = new QFormLayout;
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
    auto* editEmboss = new QPushButton(tr("Change the words..."));
    editEmboss->setToolTip(tr("Change the embossed words, their size or depth, or click to move them"));
    connect(editEmboss, &QPushButton::clicked, this, &PropertiesPanel::editEmbossRequested);
    general->addRow(editEmboss);
    m_editEmbossButton = editEmboss;
    auto* editEdges = new QPushButton(tr("Change the edges..."));
    editEdges->setToolTip(tr("Change the kind or size, or pick other edges"));
    connect(editEdges, &QPushButton::clicked, this, &PropertiesPanel::editEdgesRequested);
    general->addRow(editEdges);
    m_editEdgesButton = editEdges;
    m_throughAll = new QCheckBox(tr("Through all"));
    m_throughAll->setToolTip(tr("Straight tools (box, cylinder, tube, text) reach right through the shape along "
                                "their own height, whatever size the shape becomes"));
    connect(m_throughAll, &QCheckBox::toggled, this, [this](bool on) {
        if (m_body)
            emit throughAllRequested(m_body->id, on);
    });
    general->addRow(m_throughAll);
    m_hardware = new QCheckBox(tr("Joint part (keeps its size when rescaled)"));
    m_hardware->setToolTip(tr("For a ball, peg, pin or other part that has to fit something bought or printed at "
                              "a set size: Rescale Design (Tools) moves it with the design but leaves its size "
                              "alone. Balls, pegs and stand-in magnets from Quick Socket, Quick Hinge's pieces "
                              "and Split's pegs are joint parts already."));
    connect(m_hardware, &QCheckBox::toggled, this, [this](bool on) {
        if (m_body)
            emit hardwareRequested(m_body->id, on);
    });
    general->addRow(m_hardware);
    m_keep = new QDoubleSpinBox;
    m_keep->setRange(0.1, 100.0);
    m_keep->setDecimals(1);
    m_keep->setSingleStep(5.0);
    m_keep->setSuffix(tr(" % of the triangles"));
    m_keep->setKeyboardTracking(false);  // simplifying a big mesh takes a moment: once, when done typing
    m_keep->setToolTip(tr("How many of the shape's triangles to keep. Fewer is lighter to print and send, "
                          "but loses fine detail."));
    connect(m_keep, &QDoubleSpinBox::valueChanged, this, [this](double percent) {
        if (m_body)
            emit simplifyKeepRequested(m_body->id, percent / 100.0);
    });
    m_keepRow = m_keep;
    m_keepLabel = new QLabel(tr("Keep:"));
    general->addRow(m_keepLabel, m_keep);
    m_keepCount = new QSpinBox;
    m_keepCount->setRange(100, 100);
    m_keepCount->setSingleStep(1000);
    m_keepCount->setGroupSeparatorShown(true);
    m_keepCount->setKeyboardTracking(false);
    m_keepCount->setToolTip(tr("The same, as a number of triangles to aim for (it ends up close to this)"));
    connect(m_keepCount, &QSpinBox::valueChanged, this, [this](int count) {
        if (m_body && m_simplifyFrom > 0)
            emit simplifyKeepRequested(m_body->id, std::clamp(double(count) / double(m_simplifyFrom), 0.001, 1.0));
    });
    m_keepCountLabel = new QLabel(tr("Triangles:"));
    general->addRow(m_keepCountLabel, m_keepCount);
    m_keepSize = new QLabel;
    m_keepSize->setWordWrap(true);
    general->addRow(m_keepSize);
    m_keepCount->hide();
    m_keepCountLabel->hide();
    m_keepSize->hide();
    m_strength = new QSpinBox;
    m_strength->setRange(1, 10);
    m_strength->setKeyboardTracking(false);  // smoothing takes a moment: once, when done typing
    m_strength->setToolTip(tr("How strongly to smooth: 1 takes the edge off, 10 rounds the shape right off"));
    connect(m_strength, &QSpinBox::valueChanged, this, [this](int strength) {
        if (m_body)
            emit smoothStrengthRequested(m_body->id, strength);
    });
    m_strengthLabel = new QLabel(tr("Strength:"));
    general->addRow(m_strengthLabel, m_strength);
    layout->addLayout(general);

    // A Text shape's words
    auto* textGroup = new QGroupBox(tr("Text"));
    m_textGroup = textGroup;
    auto* textLayout = new QVBoxLayout(textGroup);
    m_words = new WordsEditor;
    textLayout->addWidget(m_words);
    auto* textForm = new QFormLayout;
    textForm->setContentsMargins(0, 4, 0, 0);
    m_letterHeight = makeSpin(0.5, kMaxLength, tr(" mm"), 2,
                              tr("How tall a capital letter is. Changes the width and depth to match"));
    textForm->addRow(tr("Letter height:"), m_letterHeight);
    textLayout->addLayout(textForm);
    layout->addWidget(textGroup);

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
    page->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);  // not shown yet

    connect(m_name, &QLineEdit::editingFinished, this, &PropertiesPanel::commitName);
    for (std::size_t i = 0; i < m_sizeFields.size(); ++i)
        connect(m_sizeFields[i], &QDoubleSpinBox::valueChanged, this, [this, i] { commitSpec(i); });
    connect(m_uniform, &QCheckBox::toggled, this, &PropertiesPanel::uniformChanged);
    connect(m_words, &WordsEditor::changed, this, &PropertiesPanel::commitWords);
    connect(m_letterHeight, &QDoubleSpinBox::valueChanged, this, &PropertiesPanel::commitLetterHeight);
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

    // Sizes and positions can follow the design variables: press = in one, or right-click it.
    for (QDoubleSpinBox* spin : {m_sizeFields[0], m_sizeFields[1], m_sizeFields[2], m_sizeFields[3], m_sizeFields[4],
                                 m_x, m_y, m_z, m_rotX, m_rotY, m_rotZ}) {
        spin->installEventFilter(this);
        if (auto* edit = spin->findChild<QLineEdit*>())
            edit->installEventFilter(this);
    }

    setBody(nullptr);
}

QDoubleSpinBox* PropertiesPanel::spinOf(QObject* watched) const
{
    for (QDoubleSpinBox* spin : {m_sizeFields[0], m_sizeFields[1], m_sizeFields[2], m_sizeFields[3], m_sizeFields[4],
                                 m_x, m_y, m_z, m_rotX, m_rotY, m_rotZ})
        if (watched == spin || (watched && watched->parent() == spin))
            return spin;
    return nullptr;
}

QString PropertiesPanel::keyOf(const QDoubleSpinBox* spin) const
{
    if (!m_body)
        return {};
    const std::array<std::pair<const QDoubleSpinBox*, const char*>, 6> placed{
        {{m_x, "at.x"}, {m_y, "at.y"}, {m_z, "at.z"}, {m_rotX, "at.rotationX"}, {m_rotY, "at.rotationY"},
         {m_rotZ, "at.rotationZ"}}};
    for (const auto& [s, key] : placed)
        if (s == spin)
            return QString::fromLatin1(key);
    if (m_body->isBoolean())
        return {};
    const auto& fields = specFields(m_body->spec);
    for (std::size_t i = 0; i < m_sizeFields.size() && i < fields.size(); ++i)
        if (m_sizeFields[i] == spin)
            return QString::fromLatin1(fields[i].key);
    return {};
}

QString PropertiesPanel::labelOf(const QDoubleSpinBox* spin) const
{
    return m_body ? QString::fromStdString(variables::fieldLabel(*m_body, keyOf(spin).toStdString())) : QString();
}

bool PropertiesPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::KeyPress && event->type() != QEvent::ContextMenu)
        return QWidget::eventFilter(watched, event);
    QDoubleSpinBox* spin = spinOf(watched);
    const QString key = spin ? keyOf(spin) : QString();
    if (!spin || key.isEmpty() || !m_body)
        return QWidget::eventFilter(watched, event);
    const ObjectId id = m_body->id;
    if (event->type() == QEvent::KeyPress) {
        if (static_cast<QKeyEvent*>(event)->text() != QLatin1String("="))
            return QWidget::eventFilter(watched, event);
        emit formulaRequested(id, key, labelOf(spin));
        return true;
    }
    const bool following = m_body->formulas.count(key.toStdString()) > 0;
    QMenu menu;
    QAction* use = menu.addAction(following ? tr("Change the Formula... (=)") : tr("Use a Variable... (=)"));
    QAction* stop = following ? menu.addAction(tr("Stop Following the Variables")) : nullptr;
    QAction* chosen = menu.exec(static_cast<QContextMenuEvent*>(event)->globalPos());
    if (chosen == use)
        emit formulaRequested(id, key, labelOf(spin));
    else if (chosen && chosen == stop)
        emit formulaClearRequested(id, key);
    return true;
}

void PropertiesPanel::showFormulas()
{
    for (QDoubleSpinBox* spin : {m_sizeFields[0], m_sizeFields[1], m_sizeFields[2], m_sizeFields[3], m_sizeFields[4],
                                 m_x, m_y, m_z, m_rotX, m_rotY, m_rotZ}) {
        if (!spin->property("baseTip").isValid() || spin->property("tipFrom").toString() != spin->toolTip())
            spin->setProperty("baseTip", spin->toolTip());
        const QString base = spin->property("baseTip").toString();
        const QString key = keyOf(spin);
        const auto found = m_body && !key.isEmpty() ? m_body->formulas.find(key.toStdString())
                                                    : decltype(m_body->formulas)::const_iterator{};
        const bool following = m_body && !key.isEmpty() && found != m_body->formulas.end();
        QString tip = base;
        if (following)
            tip = tr("= %1 (from the design variables). Press = to change it; type a number to stop following "
                     "it.\n\n%2")
                      .arg(QString::fromStdString(found->second), base);
        else if (!key.isEmpty())
            tip = base + tr("\n\nPress = to make it follow the design variables.");
        spin->setToolTip(tip);
        spin->setProperty("tipFrom", tip);
        spin->setStyleSheet(following ? QStringLiteral("QDoubleSpinBox { background-color: rgba(52, 120, 246, 0.12); }")
                                      : QString());
    }
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
    // (A cut with a joint at it can be moved from inside the shape's history: slice 94.)
    m_editCutButton->setVisible(split && (!isPart || body->boolean->cutJoint.has_value()));
    const bool embossed = body->isBoolean() && body->boolean->op == BooleanOp::Emboss;
    m_editEmbossButton->setVisible(embossed && !isPart);
    if (auto* editEmboss = qobject_cast<QPushButton*>(m_editEmbossButton); embossed && editEmboss) {
        const bool picture = body->boolean->picture.has_value();
        editEmboss->setText(picture ? tr("Change the picture...") : tr("Change the words..."));
        editEmboss->setToolTip(picture ? tr("Change the embossed picture, its size or depth, or click to "
                                                    "move it")
                                               : tr("Change the embossed words, their size or depth, or click to "
                                                    "move them"));
    }
    m_editEdgesButton->setVisible(body->isBoolean() && body->boolean->op == BooleanOp::Edges && !isPart);
    {
        const QSignalBlocker block(m_hardware);
        m_hardware->setChecked(body->hardware);
    }
    const bool subtract = body->isBoolean() && body->boolean->op == BooleanOp::Subtract;
    m_throughAll->setVisible(subtract);
    if (subtract) {
        const QSignalBlocker block(m_throughAll);
        m_throughAll->setChecked(body->boolean->throughAll);
    }
    const bool simplify = body->isBoolean() && body->boolean->op == BooleanOp::Simplify && body->boolean->simplify;
    m_keepRow->setVisible(simplify && !isPart);
    m_keepLabel->setVisible(simplify && !isPart);
    if (!simplify)
        setSimplifyCounts(0, 0);
    const bool smooth = body->isBoolean() && body->boolean->op == BooleanOp::Smooth && body->boolean->smooth;
    m_strength->setVisible(smooth && !isPart);
    m_strengthLabel->setVisible(smooth && !isPart);
    if (smooth) {
        const QSignalBlocker block(m_strength);
        m_strength->setValue(body->boolean->smooth->strength);
    }
    if (simplify) {
        const QSignalBlocker block(m_keep);
        m_keep->setValue(body->boolean->simplify->keep * 100.0);
    }
    const auto* textSpec = body->isBoolean() ? nullptr : std::get_if<TextSpec>(&body->spec);
    m_textGroup->setVisible(textSpec != nullptr);
    if (textSpec) {
        m_words->setWords(textSpec->text, textSpec->font, textSpec->bold, textSpec->italic, textSpec->vertical);
        const QSignalBlocker block(m_letterHeight);
        try {
            m_letterHeight->setValue(text::letterHeightOf(*textSpec));
        } catch (const std::exception&) {
        }
    }
    if (auto* button = qobject_cast<QPushButton*>(m_separateButton); button && body->isBoolean()) {
        switch (body->boolean->op) {
        case BooleanOp::Split:
            button->setText(tr("Remove the cut"));
            button->setToolTip(tr("Give back the whole shape, where it is"));
            break;
        case BooleanOp::Mirror:
            button->setText(tr("Remove the mirror"));
            button->setToolTip(tr("Give back the shape the right way round, where it is"));
            break;
        case BooleanOp::Edges:
            button->setText(tr("Sharp edges again"));
            button->setToolTip(tr("Give back the shape with its edges as they were"));
            break;
        case BooleanOp::Emboss:
            if (body->boolean->picture) {
                button->setText(tr("Remove the picture"));
                button->setToolTip(tr("Give back the shape without the embossed picture, where it is"));
            } else {
                button->setText(tr("Remove the words"));
                button->setToolTip(tr("Give back the shape without the embossed words, where it is"));
            }
            break;
        case BooleanOp::Smooth:
            button->setText(tr("Remove the smoothing"));
            button->setToolTip(tr("Give back the shape as it was before it was smoothed, where it is"));
            break;
        case BooleanOp::ExtrudeDown:
            button->setText(tr("Remove the extrusion"));
            button->setToolTip(tr("Give back the shape without what was added below it, where it is"));
            break;
        case BooleanOp::Simplify:
            button->setText(tr("Remove the simplifying"));
            button->setToolTip(tr("Give back the shape with all its triangles, where it is"));
            break;
        case BooleanOp::Sculpt:
            button->setText(tr("Remove the sculpting"));
            button->setToolTip(tr("Give back the shape as it was before it was sculpted, where it is"));
            break;
        case BooleanOp::MaskSplit:
            button->setText(tr("Remove the cut"));
            button->setToolTip(tr("Give back the whole shape, where it is"));
            break;
        case BooleanOp::PanelLines:
            button->setText(tr("Remove the panel lines"));
            button->setToolTip(tr("Give back the shape without them, where it is"));
            break;
        case BooleanOp::Stamp:
            button->setText(tr("Remove the stamps"));
            button->setToolTip(tr("Give back the shape without them, where it is"));
            break;
        case BooleanOp::Hollow:
            button->setText(tr("Remove the hollowing"));
            button->setToolTip(tr("Give back the shape solid, where it is"));
            break;
        case BooleanOp::Socket:
            button->setText(tr("Remove the socket"));
            button->setToolTip(tr("Give back the shape without the socket, where it is"));
            break;
        case BooleanOp::Hinge:
            button->setText(tr("Remove the joint"));
            button->setToolTip(tr("Give back the shape without this half of the joint, where it is"));
            break;
        case BooleanOp::Linked:
            button->setText(tr("Unlink"));
            button->setToolTip(tr("Make it a shape of its own, as it is now: it stops following the other side"));
            break;
        case BooleanOp::Group:
            button->setText(tr("Ungroup"));
            button->setToolTip(tr("Let the shapes in this group move on their own again, where they are"));
            break;
        default:
            button->setText(tr("Separate into parts"));
            button->setToolTip(tr("Break this combined shape back into its parts, each left where it is"));
            break;
        }
    }
    m_sizeGroup->setVisible(!body->isBoolean());  // a combined shape is sized through its parts
    m_sitButton->setVisible(!isPart);

    if (!body->isBoolean()) {
        const auto& fields = specFields(body->spec);
        for (std::size_t i = 0; i < m_sizeFields.size(); ++i) {
            const bool used = i < fields.size();
            showRow(m_sizeForm, m_sizeFields[i], used);
            if (!used)
                continue;
            m_sizeLabels[i]->setText(tr(fields[i].label) + QLatin1Char(':'));
            m_sizeFields[i]->setToolTip(tr(fields[i].tip));
            m_sizeFields[i]->setMinimum(fields[i].allowZero ? 0.0 : kMinLength);
            m_sizeFields[i]->setValue(specValue(body->spec, i));
        }
        const bool roundable = hasRoundGroups(body->spec);
        showRow(m_sizeForm, m_round, roundable);
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
    showFormulas();
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

void PropertiesPanel::commitSpec(std::size_t i)
{
    if (!m_body || i >= specFields(m_body->spec).size())
        return;
    // Only the field the user changed counts. (Comparing every field would also catch
    // one the box couldn't show exactly, e.g. a 0.003 mm import shown as its 0.1 mm
    // minimum, and change that one instead.)
    PrimitiveSpec spec = m_body->spec;
    const bool uniform = m_uniform->isChecked();
    bool changed = false;
    {
        const double old = specValue(spec, i), v = m_sizeFields[i]->value();
        if (!differs(old, v))
            return;
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
    }
    if (changed)
        emit specRequested(m_body->id, spec);
}

void PropertiesPanel::commitWords()
{
    if (!m_body || m_body->isBoolean())
        return;
    const auto* old = std::get_if<TextSpec>(&m_body->spec);
    if (!old)
        return;
    TextSpec next;
    try {
        next = text::withText(*old, m_words->text(), m_words->font(), m_words->bold(), m_words->italic(),
                              m_words->vertical());
    } catch (const std::exception&) {
        // No letters (e.g. the box was emptied): show the words it still has.
        m_words->setWords(old->text, old->font, old->bold, old->italic, old->vertical);
        return;
    }
    if (!(next == *old))
        emit specRequested(m_body->id, next);
}

void PropertiesPanel::commitLetterHeight()
{
    if (!m_body || m_body->isBoolean())
        return;
    const auto* old = std::get_if<TextSpec>(&m_body->spec);
    if (!old)
        return;
    try {
        const double now = text::letterHeightOf(*old);
        const double wanted = m_letterHeight->value();
        if (!(now > 0.0) || !differs(now, wanted))
            return;
        TextSpec next = *old;
        next.width *= wanted / now;
        next.depth *= wanted / now;
        emit specRequested(m_body->id, next);
    } catch (const std::exception&) {
    }
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

void PropertiesPanel::setSimplifyCounts(std::size_t from, std::size_t now)
{
    m_simplifyFrom = from;
    const bool shown = from > 0 && !m_keepRow->isHidden();
    m_keepCount->setVisible(shown);
    m_keepCountLabel->setVisible(shown);
    m_keepSize->setVisible(shown);
    if (!shown)
        return;
    {
        const QSignalBlocker block(m_keepCount);
        m_keepCount->setRange(int(std::min<std::size_t>(100, from)), int(std::min<std::size_t>(from, 2'000'000'000)));
        m_keepCount->setValue(int(std::min<std::size_t>(now, 2'000'000'000)));
    }
    // A binary STL: 84 bytes, then 50 a triangle.
    const auto megabytes = [](std::size_t tris) {
        const double mb = (84.0 + 50.0 * double(tris)) / (1024.0 * 1024.0);
        return QString::number(mb, 'f', mb < 10.0 ? 1 : 0);
    };
    m_keepSize->setText(tr("As an STL file: about %1 MB (was %2 MB).").arg(megabytes(now), megabytes(from)));
}

}  // namespace forge::app
