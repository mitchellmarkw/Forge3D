// SPDX-License-Identifier: GPL-3.0-or-later
#include "ReferencePanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>

namespace forge::app {
namespace {

// The app's preferences (see MainWindow.cpp: always named, so Windows has somewhere to keep them).
class Preferences : public QSettings {
public:
    Preferences() : QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"),
                              QStringLiteral("Forge3D"))
    {
    }
};

QDoubleSpinBox* number(double low, double high, int decimals, const QString& suffix, const QString& tip)
{
    auto* box = new QDoubleSpinBox;
    box->setRange(low, high);
    box->setDecimals(decimals);
    box->setSuffix(suffix);
    box->setToolTip(tip);
    box->setKeyboardTracking(false);  // a change per finished number, not per key
    return box;
}

// A label sitting right on top of its combo box.
QWidget* labelled(const QString& text, QComboBox* combo)
{
    auto* box = new QWidget;
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(new QLabel(text));
    column->addWidget(combo);
    return box;
}

}  // namespace

ReferencePanel::ReferencePanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel(tr("Reference Images"));
    QFont bold = title->font();
    bold.setBold(true);
    title->setFont(bold);
    layout->addWidget(title);
    auto* help = new QLabel(tr("A photo or drawing for each side, to build the model to. Pictures stay in the "
                               "file, but are never exported or printed. Drag a picture to move it."));
    help->setWordWrap(true);
    help->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(help);
    layout->addSpacing(4);

    m_direction = new QComboBox;
    for (int v = 0; v < kRefViewCount; ++v)
        m_direction->addItem(tr(reference::title(static_cast<RefView>(v))), v);
    m_direction->setToolTip(tr("The side the picture is seen from: Front is seen from the front, Top from above"));
    layout->addWidget(labelled(tr("Direction:"), m_direction));

    m_file = new QLabel;
    m_file->setWordWrap(true);
    layout->addWidget(m_file);
    auto* fileRow = new QHBoxLayout;
    m_add = new QPushButton(tr("Add Picture..."));
    m_add->setToolTip(tr("Choose a picture file (PNG, JPEG, BMP...) for this side"));
    m_remove = new QPushButton(tr("Remove"));
    m_remove->setToolTip(tr("Take this side's picture away"));
    fileRow->addWidget(m_add);
    fileRow->addWidget(m_remove);
    layout->addLayout(fileRow);
    m_look = new QPushButton(tr("Look From This Side"));
    m_look->setToolTip(tr("Turn the view to look straight at the picture"));
    layout->addWidget(m_look);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    m_status->hide();
    layout->addWidget(m_status);

    m_settings = new QWidget;
    auto* settings = new QVBoxLayout(m_settings);
    settings->setContentsMargins(0, 0, 0, 0);

    auto* checks = new QHBoxLayout;
    m_shown = new QCheckBox(tr("Show"));
    m_shown->setToolTip(tr("Show or hide the picture (it stays in the file either way)"));
    m_active = new QCheckBox(tr("Active"));
    m_active->setToolTip(tr("An active picture can be clicked and dragged. Untick it to click straight "
                            "through the picture, to the shapes and the plate behind it."));
    checks->addWidget(m_shown);
    checks->addWidget(m_active);
    checks->addStretch(1);
    settings->addLayout(checks);

    auto* opacityRow = new QHBoxLayout;
    opacityRow->addWidget(new QLabel(tr("Opacity:")));
    m_opacity = new QSlider(Qt::Horizontal);
    m_opacity->setRange(5, 100);
    m_opacity->setToolTip(tr("How solid the picture looks: less lets the grid and shapes show through"));
    m_opacityValue = new QLabel;
    m_opacityValue->setMinimumWidth(m_opacityValue->fontMetrics().horizontalAdvance(QStringLiteral("100%")));
    opacityRow->addWidget(m_opacity, 1);
    opacityRow->addWidget(m_opacityValue);
    settings->addLayout(opacityRow);

    auto* place = new QGroupBox(tr("Size and place"));
    auto* grid = new QGridLayout(place);
    m_width = number(0.01, 100000, 2, tr(" mm"), tr("How wide the picture is on the model"));
    m_height = new QLabel;
    m_across = number(-100000, 100000, 2, tr(" mm"), tr("Slide the picture right (or left, below 0) as seen from its side"));
    m_up = number(-100000, 100000, 2, tr(" mm"), tr("Slide the picture up (or down, below 0) as seen from its side"));
    m_behind = number(-100000, 100000, 2, tr(" mm"),
                      tr("How far behind the middle of the plate it stands. Keep it behind the model so the "
                         "model shows in front of it."));
    m_turn = number(-360, 360, 1, QStringLiteral("°"), tr("Turn the picture (anticlockwise as you see it), for a photo taken a little crooked"));
    int row = 0;
    grid->addWidget(new QLabel(tr("Width:")), row, 0);
    grid->addWidget(m_width, row++, 1);
    grid->addWidget(new QLabel(tr("Height:")), row, 0);
    grid->addWidget(m_height, row++, 1);
    grid->addWidget(new QLabel(tr("Right:")), row, 0);
    grid->addWidget(m_across, row++, 1);
    grid->addWidget(new QLabel(tr("Up:")), row, 0);
    grid->addWidget(m_up, row++, 1);
    grid->addWidget(new QLabel(tr("Behind:")), row, 0);
    grid->addWidget(m_behind, row++, 1);
    grid->addWidget(new QLabel(tr("Turn:")), row, 0);
    grid->addWidget(m_turn, row++, 1);
    settings->addWidget(place);

    auto* sizing = new QGroupBox(tr("Size from two points"));
    auto* sizingColumn = new QVBoxLayout(sizing);
    auto* sizingHelp = new QLabel(tr("Type a length you know, then click its two ends on the picture. A real "
                                     "length (a car's 4.5 m) with the model's scale (1:48) is shrunk to suit; a "
                                     "length already to scale (91.2 mm) needs scale 1."));
    sizingHelp->setWordWrap(true);
    sizingColumn->addWidget(sizingHelp);
    m_length = number(0.001, 1000000, 3, QString(), tr("The length between the two points"));
    m_unit = new QComboBox;
    m_unit->addItems({tr("mm"), tr("cm"), tr("m"), tr("inches"), tr("feet")});
    m_unit->setToolTip(tr("The unit the length is in"));
    m_scale = number(1, 10000, 2, QString(), tr("The model's scale: 1 for full size, 12 for 1:12, 48 for 1:48..."));
    {
        // Each name sits right on top of its box.
        auto* row = new QHBoxLayout;
        auto* lengthBox = new QWidget;
        auto* lengthColumn = new QVBoxLayout(lengthBox);
        lengthColumn->setContentsMargins(0, 0, 0, 0);
        lengthColumn->setSpacing(0);
        lengthColumn->addWidget(new QLabel(tr("Length:")));
        lengthColumn->addWidget(m_length);
        row->addWidget(lengthBox, 1);
        row->addWidget(labelled(tr("Unit:"), m_unit), 1);
        sizingColumn->addLayout(row);
        auto* scaleBox = new QWidget;
        auto* scaleColumn = new QVBoxLayout(scaleBox);
        scaleColumn->setContentsMargins(0, 0, 0, 0);
        scaleColumn->setSpacing(0);
        scaleColumn->addWidget(new QLabel(tr("Model scale 1 :")));
        scaleColumn->addWidget(m_scale);
        sizingColumn->addWidget(scaleBox);
    }
    m_modelLength = new QLabel;
    m_modelLength->setWordWrap(true);
    m_modelLength->setStyleSheet(QStringLiteral("color: palette(mid);"));
    sizingColumn->addWidget(m_modelLength);
    m_pick = new QPushButton(tr("Pick Two Points"));
    m_pick->setToolTip(tr("Click the first end of the length on the picture, then the other"));
    sizingColumn->addWidget(m_pick);
    settings->addWidget(sizing);
    layout->addWidget(m_settings);

    auto* done = new QPushButton(tr("Done"));
    done->setDefault(true);
    done->setToolTip(tr("Close this panel. The pictures stay."));
    layout->addWidget(done);
    layout->addStretch(1);

    {
        Preferences prefs;
        m_length->setValue(prefs.value(QStringLiteral("reference/length"), 100.0).toDouble());
        m_unit->setCurrentIndex(std::clamp(prefs.value(QStringLiteral("reference/unit"), 0).toInt(), 0, 4));
        m_scale->setValue(prefs.value(QStringLiteral("reference/scale"), 1.0).toDouble());
    }
    refreshLength();

    connect(m_direction, &QComboBox::currentIndexChanged, this,
            [this] { emit directionChanged(direction()); });
    connect(m_add, &QPushButton::clicked, this, &ReferencePanel::addRequested);
    connect(m_remove, &QPushButton::clicked, this, &ReferencePanel::removeRequested);
    connect(m_look, &QPushButton::clicked, this, &ReferencePanel::lookRequested);
    connect(m_shown, &QCheckBox::toggled, this, [this](bool on) {
        emit edited(on ? tr("Show Picture") : tr("Hide Picture"), false);
    });
    connect(m_active, &QCheckBox::toggled, this, [this](bool on) {
        emit edited(on ? tr("Make Picture Active") : tr("Make Picture Inactive"), false);
    });
    connect(m_opacity, &QSlider::valueChanged, this, [this](int value) {
        m_opacityValue->setText(QStringLiteral("%1%").arg(value));
        emit edited(tr("Picture Opacity"), true);
    });
    connect(m_width, &QDoubleSpinBox::valueChanged, this, [this](double w) {
        m_height->setText(tr("%1 mm").arg(w * m_aspect, 0, 'f', 2));
        emit edited(tr("Picture Size"), true);
    });
    for (QDoubleSpinBox* box : {m_across, m_up, m_behind})
        connect(box, &QDoubleSpinBox::valueChanged, this, [this] { emit edited(tr("Move Picture"), true); });
    connect(m_turn, &QDoubleSpinBox::valueChanged, this, [this] { emit edited(tr("Turn Picture"), true); });
    connect(m_pick, &QPushButton::clicked, this, &ReferencePanel::pickRequested);
    const auto remember = [this] {
        Preferences prefs;
        prefs.setValue(QStringLiteral("reference/length"), m_length->value());
        prefs.setValue(QStringLiteral("reference/unit"), m_unit->currentIndex());
        prefs.setValue(QStringLiteral("reference/scale"), m_scale->value());
        refreshLength();
    };
    connect(m_length, &QDoubleSpinBox::valueChanged, this, remember);
    connect(m_scale, &QDoubleSpinBox::valueChanged, this, remember);
    connect(m_unit, &QComboBox::currentIndexChanged, this, remember);
    connect(done, &QPushButton::clicked, this, &ReferencePanel::doneRequested);
}

RefView ReferencePanel::direction() const
{
    return static_cast<RefView>(std::clamp(m_direction->currentData().toInt(), 0, kRefViewCount - 1));
}

void ReferencePanel::setDirection(RefView view)
{
    m_direction->setCurrentIndex(m_direction->findData(static_cast<int>(view)));
}

void ReferencePanel::showPicture(const ReferenceImage* image, const std::vector<RefView>& taken)
{
    for (int v = 0; v < kRefViewCount; ++v) {
        const bool has = std::find(taken.begin(), taken.end(), static_cast<RefView>(v)) != taken.end();
        const QString name = tr(reference::title(static_cast<RefView>(v)));
        m_direction->setItemText(m_direction->findData(v), has ? tr("%1 (has a picture)").arg(name) : name);
    }
    m_settings->setVisible(image != nullptr);
    m_remove->setEnabled(image != nullptr);
    m_add->setText(image ? tr("Replace Picture...") : tr("Add Picture..."));
    if (!image) {
        m_file->setText(tr("<i>No picture for this side yet.</i>"));
        return;
    }
    m_file->setText(tr("<b>%1</b> (%2 x %3 pixels)")
                        .arg(QFileInfo(QString::fromStdString(image->name)).fileName().toHtmlEscaped())
                        .arg(image->pixelsWide)
                        .arg(image->pixelsHigh));
    m_aspect = image->pixelsWide > 0 ? double(image->pixelsHigh) / image->pixelsWide : 1.0;
    const QSignalBlocker b1(m_shown), b2(m_active), b3(m_opacity), b4(m_width), b5(m_across), b6(m_up),
        b7(m_behind), b8(m_turn);
    m_shown->setChecked(image->shown);
    m_active->setChecked(image->active);
    m_opacity->setValue(int(std::lround(image->opacity * 100)));
    m_opacityValue->setText(QStringLiteral("%1%").arg(m_opacity->value()));
    m_width->setValue(image->widthMm());
    m_height->setText(tr("%1 mm").arg(image->heightMm(), 0, 'f', 2));
    m_across->setValue(image->across);
    m_up->setValue(image->up);
    m_behind->setValue(image->depth);
    m_turn->setValue(image->turn);
}

double ReferencePanel::widthMm() const { return m_width->value(); }
double ReferencePanel::across() const { return m_across->value(); }
double ReferencePanel::up() const { return m_up->value(); }
double ReferencePanel::behind() const { return m_behind->value(); }
double ReferencePanel::turn() const { return m_turn->value(); }
double ReferencePanel::opacity() const { return m_opacity->value() / 100.0; }
bool ReferencePanel::shown() const { return m_shown->isChecked(); }
bool ReferencePanel::active() const { return m_active->isChecked(); }

double ReferencePanel::calibrationLength() const
{
    return reference::modelLength(m_length->value(), static_cast<reference::Unit>(m_unit->currentIndex()),
                                  m_scale->value());
}

void ReferencePanel::refreshLength()
{
    m_modelLength->setText(m_scale->value() == 1.0
                               ? tr("Full size: %1 mm on the model.").arg(calibrationLength(), 0, 'f', 2)
                               : tr("At 1:%1, that's %2 mm on the model.")
                                     .arg(m_scale->value())
                                     .arg(calibrationLength(), 0, 'f', 2));
}

void ReferencePanel::setPicking(int stage)
{
    m_pick->setText(stage == 0 ? tr("Pick Two Points") : tr("Stop Picking"));
    if (stage == 1)
        setStatus(tr("Click the first end of the length on the picture."), false);
    else if (stage == 2)
        setStatus(tr("Now click the other end."), false);
}

void ReferencePanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setVisible(!text.isEmpty());
    m_status->setStyleSheet(problem ? QStringLiteral("color: #c0392b;") : QString());
}

}  // namespace forge::app
