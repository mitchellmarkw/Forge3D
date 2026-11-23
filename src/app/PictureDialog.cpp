// SPDX-License-Identifier: GPL-3.0-or-later
#include "PictureDialog.h"

#include "forge/core/Document.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>

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

QImage imageOf(const render::Image& pic)
{
    QImage img(pic.width, pic.height, QImage::Format_ARGB32);
    for (int y = 0; y < pic.height; ++y)
        std::copy_n(pic.pixels.data() + std::size_t(y) * std::size_t(pic.width), pic.width,
                    reinterpret_cast<std::uint32_t*>(img.scanLine(y)));
    return img;
}

// A checkerboard, to show what's see-through.
void checker(QPainter& p, const QRect& r)
{
    const int k = 10;
    for (int y = r.top(); y < r.bottom(); y += k)
        for (int x = r.left(); x < r.right(); x += k)
            p.fillRect(QRect(x, y, k, k), ((x / k + y / k) & 1) ? QColor(0xe6, 0xe6, 0xe6) : Qt::white);
}

}  // namespace

namespace {

std::vector<render::Item> itemsWaiting(const Document& doc, const std::vector<ObjectId>& ids)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    struct Restore {
        ~Restore() { QApplication::restoreOverrideCursor(); }
    } restore;
    return render::itemsOf(doc, ids);
}

}  // namespace

PictureDialog::PictureDialog(const Document& doc, const std::vector<ObjectId>& ids, const QString& name, QWidget* parent)
    : PictureDialog(itemsWaiting(doc, ids), name, parent)
{
}

PictureDialog::PictureDialog(std::vector<render::Item> items, const QString& name, QWidget* parent)
    : QDialog(parent), m_items(std::move(items)), m_name(name)
{
    setWindowTitle(tr("Picture"));
    resize(980, 640);

    auto* layout = new QHBoxLayout(this);
    auto* left = new QVBoxLayout;
    layout->addLayout(left, 0);
    const QSettings prefs = preferences();

    m_kind = new QComboBox;
    m_kind->addItem(tr("A picture (PNG)"));
    m_kind->addItem(tr("A turntable, going once round (GIF)"));
    m_kind->setCurrentIndex(prefs.value(QStringLiteral("picture/kind"), 0).toInt() == 1 ? 1 : 0);
    left->addWidget(labelled(tr("Make:"), m_kind));

    m_size = new QComboBox;
    m_size->setToolTip(tr("The picture's size in pixels"));
    left->addWidget(labelled(tr("Size:"), m_size));

    m_background = new QComboBox;
    for (int i = 0; i < render::kBackgroundCount; ++i)
        m_background->addItem(tr(render::title(render::Background(i))));
    m_background->setCurrentIndex(
        std::clamp(prefs.value(QStringLiteral("picture/background"), 1).toInt(), 0, render::kBackgroundCount - 1));
    m_background->setToolTip(tr("Behind the model. Clear has no background, for putting it on your own (a GIF's "
                                "edges are hard, as GIFs are)."));
    left->addWidget(labelled(tr("Background:"), m_background));

    auto* form = new QFormLayout;
    m_turn = new QDoubleSpinBox;
    m_turn->setRange(-360.0, 360.0);
    m_turn->setDecimals(0);
    m_turn->setSingleStep(15.0);
    m_turn->setSuffix(tr("°"));
    m_turn->setWrapping(true);
    m_turn->setValue(prefs.value(QStringLiteral("picture/turn"), 30.0).toDouble());
    m_turn->setToolTip(tr("How far round from the front it's seen (0: straight on; 90: from its left side). A "
                          "turntable starts here."));
    form->addRow(tr("Seen from:"), m_turn);
    m_tilt = new QDoubleSpinBox;
    m_tilt->setRange(-30.0, 80.0);
    m_tilt->setDecimals(0);
    m_tilt->setSingleStep(5.0);
    m_tilt->setSuffix(tr("°"));
    m_tilt->setValue(prefs.value(QStringLiteral("picture/tilt"), 15.0).toDouble());
    m_tilt->setToolTip(tr("How far above it the camera is (0: level with its middle)"));
    form->addRow(tr("Looking down:"), m_tilt);
    m_fill = new QSpinBox;
    m_fill->setRange(30, 100);
    m_fill->setSingleStep(5);
    m_fill->setSuffix(tr(" %"));
    m_fill->setValue(prefs.value(QStringLiteral("picture/fill"), 85).toInt());
    m_fill->setToolTip(tr("How much of the picture the model fills"));
    form->addRow(tr("Model fills:"), m_fill);
    left->addLayout(form);
    m_shadow = new QCheckBox(tr("A soft shadow on the ground"));
    m_shadow->setChecked(prefs.value(QStringLiteral("picture/shadow"), true).toBool());
    left->addWidget(m_shadow);

    m_gifBox = new QWidget;
    auto* gif = new QVBoxLayout(m_gifBox);
    gif->setContentsMargins(0, 0, 0, 0);
    m_frames = new QComboBox;
    for (int n : {24, 36, 48, 72})
        m_frames->addItem(tr("%1 (every %2°)").arg(n).arg(360 / n), n);
    m_frames->setCurrentIndex(std::clamp(prefs.value(QStringLiteral("picture/frames"), 1).toInt(), 0, 3));
    m_frames->setToolTip(tr("More is smoother, and a bigger file"));
    gif->addWidget(labelled(tr("Pictures in one turn:"), m_frames));
    auto* gifForm = new QFormLayout;
    m_seconds = new QDoubleSpinBox;
    m_seconds->setRange(1.0, 30.0);
    m_seconds->setDecimals(1);
    m_seconds->setSuffix(tr(" s"));
    m_seconds->setValue(prefs.value(QStringLiteral("picture/seconds"), 6.0).toDouble());
    m_seconds->setToolTip(tr("How long it takes to go once round"));
    gifForm->addRow(tr("Once round in:"), m_seconds);
    gif->addLayout(gifForm);
    m_spin = new QCheckBox(tr("Turn the preview"));
    m_spin->setToolTip(tr("See it go round (roughly) before making the GIF"));
    gif->addWidget(m_spin);
    left->addWidget(m_gifBox);

    m_status = new QLabel;
    m_status->setWordWrap(true);
    m_status->setMaximumWidth(320);
    left->addWidget(m_status);
    left->addStretch(1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    auto* saveButton = buttons->addButton(tr("Save..."), QDialogButtonBox::ActionRole);
    left->addWidget(buttons);

    m_preview = new QLabel;
    m_preview->setMinimumSize(520, 520);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setStyleSheet(QStringLiteral("border: 1px solid #c8ccd0;"));
    layout->addWidget(m_preview, 1);

    m_later = new QTimer(this);
    m_later->setSingleShot(true);
    m_later->setInterval(120);
    connect(m_later, &QTimer::timeout, this, &PictureDialog::redraw);
    m_spinTimer = new QTimer(this);
    m_spinTimer->setInterval(60);
    connect(m_spinTimer, &QTimer::timeout, this, [this] {
        m_spinAngle += 360.0 / std::max(1, m_frames->currentData().toInt());
        redraw();
    });
    connect(m_spin, &QCheckBox::toggled, this, [this](bool on) {
        m_spinAngle = 0.0;
        on ? m_spinTimer->start() : m_spinTimer->stop();
        if (!on)
            redraw();
    });
    connect(m_kind, &QComboBox::currentIndexChanged, this, [this] {
        fillSizes();
        m_gifBox->setVisible(turntable());
        if (!turntable())
            m_spin->setChecked(false);
        m_later->start();
    });
    for (QComboBox* c : {m_size, m_background, m_frames})
        connect(c, &QComboBox::currentIndexChanged, m_later, qOverload<>(&QTimer::start));
    for (QDoubleSpinBox* s : {m_turn, m_tilt})
        connect(s, &QDoubleSpinBox::valueChanged, m_later, qOverload<>(&QTimer::start));
    connect(m_fill, &QSpinBox::valueChanged, m_later, qOverload<>(&QTimer::start));
    connect(m_shadow, &QCheckBox::toggled, m_later, qOverload<>(&QTimer::start));
    connect(buttons, &QDialogButtonBox::rejected, this, [this] {
        remember();
        reject();
    });
    connect(saveButton, &QPushButton::clicked, this, &PictureDialog::save);

    fillSizes();
    m_gifBox->setVisible(turntable());
    if (m_items.empty())
        m_status->setText(tr("There's nothing to take a picture of."));
}

bool PictureDialog::turntable() const
{
    return m_kind->currentIndex() == 1;
}

void PictureDialog::fillSizes()
{
    const QSignalBlocker block(m_size);
    const QSettings prefs = preferences();
    m_size->clear();
    if (turntable()) {
        for (int s : {320, 480, 600, 800})
            m_size->addItem(tr("Square, %1 × %1").arg(s), QSize(s, s));
        m_size->setCurrentIndex(std::clamp(prefs.value(QStringLiteral("picture/gifSize"), 2).toInt(), 0, 3));
    } else {
        m_size->addItem(tr("Square, 1080 × 1080 (posts)"), QSize(1080, 1080));
        m_size->addItem(tr("Square, 2000 × 2000 (shop listings)"), QSize(2000, 2000));
        m_size->addItem(tr("Tall, 1080 × 1350"), QSize(1080, 1350));
        m_size->addItem(tr("Wide, 1920 × 1080"), QSize(1920, 1080));
        m_size->addItem(tr("Small, 800 × 800"), QSize(800, 800));
        m_size->setCurrentIndex(std::clamp(prefs.value(QStringLiteral("picture/size"), 0).toInt(), 0, 4));
    }
}

QSize PictureDialog::pictureSize() const
{
    return m_size->currentData().toSize();
}

render::Settings PictureDialog::settings(QSize size, int smooth) const
{
    render::Settings s;
    s.width = size.width();
    s.height = size.height();
    s.turn = m_turn->value();
    s.tilt = m_tilt->value();
    s.fill = m_fill->value() / 100.0;
    s.background = render::Background(m_background->currentIndex());
    s.shadow = m_shadow->isChecked();
    s.smooth = smooth;
    return s;
}

void PictureDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    m_later->start();
}

void PictureDialog::redraw()
{
    if (m_items.empty())
        return;
    // The picture's shape, fitted to the preview.
    const QSize full = pictureSize();
    const QSize room = m_preview->size() - QSize(8, 8);
    QSize size = full.scaled(room.expandedTo(QSize(64, 64)), Qt::KeepAspectRatio);
    const bool spinning = m_spinTimer->isActive();
    if (spinning)
        size = size.scaled(QSize(320, 320), Qt::KeepAspectRatio);
    render::Settings s = settings(size.expandedTo(QSize(32, 32)), spinning ? 1 : 2);
    s.turn += m_spinAngle;
    QImage canvas(m_preview->size() - QSize(2, 2), QImage::Format_ARGB32);
    canvas.fill(Qt::white);
    try {
        const QImage img = imageOf(render::picture(m_items, s));
        QPainter p(&canvas);
        const QRect at(QPoint((canvas.width() - size.width()) / 2, (canvas.height() - size.height()) / 2), size);
        if (s.background == render::Background::Clear)
            checker(p, at);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawImage(at, img);
        if (!spinning)
            m_status->setText(tr("%1 × %2 pixels.").arg(full.width()).arg(full.height()));
    } catch (const std::exception& e) {
        m_status->setText(QString::fromUtf8(e.what()));
    }
    m_preview->setPixmap(QPixmap::fromImage(canvas));
}

void PictureDialog::remember() const
{
    QSettings prefs = preferences();
    prefs.setValue(QStringLiteral("picture/kind"), m_kind->currentIndex());
    prefs.setValue(turntable() ? QStringLiteral("picture/gifSize") : QStringLiteral("picture/size"), m_size->currentIndex());
    prefs.setValue(QStringLiteral("picture/background"), m_background->currentIndex());
    prefs.setValue(QStringLiteral("picture/turn"), m_turn->value());
    prefs.setValue(QStringLiteral("picture/tilt"), m_tilt->value());
    prefs.setValue(QStringLiteral("picture/fill"), m_fill->value());
    prefs.setValue(QStringLiteral("picture/shadow"), m_shadow->isChecked());
    prefs.setValue(QStringLiteral("picture/frames"), m_frames->currentIndex());
    prefs.setValue(QStringLiteral("picture/seconds"), m_seconds->value());
}

void PictureDialog::save()
{
    if (m_items.empty())
        return;
    const bool gif = turntable();
    const QString start = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + QLatin1Char('/') +
                          m_name + (gif ? tr(" turntable.gif") : QStringLiteral(".png"));
    const QString path =
        QFileDialog::getSaveFileName(this, tr("Save Picture"), start, gif ? tr("GIF (*.gif)") : tr("PNG (*.png)"));
    if (path.isEmpty())
        return;
    remember();
    m_spin->setChecked(false);

    // Made in the background, a picture at a time, so it can be stopped.
    const render::Settings base = settings(pictureSize(), gif ? 2 : 3);
    const int frames = gif ? m_frames->currentData().toInt() : 1;
    const int delay = std::max(2, int(std::lround(m_seconds->value() * 100.0 / frames)));
    auto done = std::make_shared<std::atomic<int>>(0);
    auto stop = std::make_shared<std::atomic<bool>>(false);
    auto problem = std::make_shared<QString>();
    auto items = std::make_shared<std::vector<render::Item>>(m_items);
    QThread* worker = QThread::create([=] {
        try {
            std::vector<render::Image> pictures;
            for (int n = 0; n < frames && !*stop; ++n) {
                render::Settings s = base;
                s.turn += 360.0 * n / frames;
                pictures.push_back(render::picture(*items, s));
                ++*done;
            }
            if (*stop)
                return;
            if (gif) {
                const auto bytes = render::gif(pictures, delay);
                QFile f(path);
                if (!f.open(QIODevice::WriteOnly) || f.write(reinterpret_cast<const char*>(bytes.data()),
                                                             qint64(bytes.size())) != qint64(bytes.size()))
                    *problem = QObject::tr("the file couldn't be written");
            } else if (!imageOf(pictures.front()).save(path, "PNG")) {
                *problem = QObject::tr("the file couldn't be written");
            }
        } catch (const std::exception& e) {
            *problem = QString::fromUtf8(e.what());
        }
    });
    QProgressDialog progress(gif ? tr("Making the turntable...") : tr("Making the picture..."), tr("Stop"), 0,
                             frames + (gif ? 1 : 0), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    QTimer poll;
    connect(&poll, &QTimer::timeout, &progress, [&] { progress.setValue(std::min(done->load(), frames)); });
    connect(&progress, &QProgressDialog::canceled, &progress, [stop] { *stop = true; });
    QEventLoop wait;
    connect(worker, &QThread::finished, &wait, &QEventLoop::quit);
    worker->start();
    poll.start(100);
    wait.exec();
    poll.stop();
    worker->deleteLater();
    progress.reset();
    if (*stop)
        m_status->setText(tr("Stopped; nothing saved."));
    else if (!problem->isEmpty())
        m_status->setText(tr("It couldn't be saved: %1").arg(*problem));
    else
        m_status->setText(tr("Saved %1.").arg(QDir::toNativeSeparators(path)));
}

}  // namespace forge::app
