// SPDX-License-Identifier: GPL-3.0-or-later
#include "InstructionSheetDialog.h"

#include "forge/core/Document.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPageLayout>
#include <QPainter>
#include <QPdfWriter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

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

QImage imageOf(const illustrate::Picture& pic)
{
    QImage img(pic.width, pic.height, QImage::Format_ARGB32);
    for (int y = 0; y < pic.height; ++y)
        std::copy_n(pic.pixels.data() + std::size_t(y) * std::size_t(pic.width), pic.width,
                    reinterpret_cast<std::uint32_t*>(img.scanLine(y)));
    return img;
}

QString sizeText(const parts::Row& r)
{
    return QStringLiteral("%1 × %2 × %3 mm").arg(r.size[0], 0, 'f', 1).arg(r.size[1], 0, 'f', 1).arg(r.size[2], 0, 'f', 1);
}

}  // namespace

void paintIllustration(QPainter& p, const illustrate::Picture& pic, const QRectF& target)
{
    if (pic.width <= 0)
        return;
    const double k = std::min(target.width() / pic.width, target.height() / pic.height);
    const QRectF at(target.x() + (target.width() - pic.width * k) / 2.0,
                    target.y() + (target.height() - pic.height * k) / 2.0, pic.width * k, pic.height * k);
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(at, imageOf(pic));
    const auto map = [&](double x, double y) { return QPointF(at.x() + x * k, at.y() + y * k); };
    const double r = pic.balloon * k;
    const QColor ink(0x2b, 0x2f, 0x33);
    QFont font = p.font();
    font.setBold(true);
    font.setPixelSize(std::max(6, int(r * 1.05)));
    p.setFont(font);
    for (const auto& l : pic.labels) {
        const QPointF a = map(l.ax, l.ay), b = map(l.bx, l.by);
        p.setPen(QPen(ink, std::max(1.0, r * 0.09)));
        // The leader stops at the balloon's edge.
        const QPointF d = a - b;
        const double len = std::hypot(d.x(), d.y());
        const QPointF edge = len > r ? b + d * (r / len) : b;
        p.drawLine(a, edge);
        p.setBrush(ink);
        p.drawEllipse(a, r * 0.16, r * 0.16);
        p.setBrush(Qt::white);
        p.drawEllipse(b, r, r);
        p.drawText(QRectF(b.x() - r, b.y() - r, 2 * r, 2 * r), Qt::AlignCenter, QString::number(l.number));
    }
    p.restore();
}

InstructionSheetDialog::InstructionSheetDialog(const Document& doc, std::vector<ObjectId> ids, const QString& title,
                                               QWidget* parent)
    : QDialog(parent), m_doc(doc), m_ids(std::move(ids))
{
    setWindowTitle(tr("Instruction Sheet"));
    resize(980, 640);
    auto* layout = new QHBoxLayout(this);
    auto* left = new QVBoxLayout;
    layout->addLayout(left, 0);

    const QSettings prefs = preferences();
    auto* form = new QFormLayout;
    m_title = new QLineEdit(title);
    m_title->setToolTip(tr("Printed at the top of the sheet"));
    form->addRow(tr("Title:"), m_title);
    left->addLayout(form);

    m_view = new QComboBox;
    for (int i = 0; i < illustrate::kViewCount; ++i)
        m_view->addItem(tr(illustrate::title(illustrate::View(i))));
    m_view->setCurrentIndex(std::clamp(prefs.value(QStringLiteral("sheet/view"), 1).toInt(), 0, illustrate::kViewCount - 1));
    m_view->setToolTip(tr("Which way the exploded view looks at the parts"));
    left->addWidget(labelled(tr("View from:"), m_view));

    auto* spreadForm = new QFormLayout;
    m_spread = new QDoubleSpinBox;
    m_spread->setRange(0.0, 300.0);
    m_spread->setDecimals(0);
    m_spread->setSingleStep(10.0);
    m_spread->setSuffix(tr(" %"));
    m_spread->setValue(prefs.value(QStringLiteral("sheet/spread"), 60.0).toDouble());
    m_spread->setToolTip(tr("How far the parts are pulled apart, out from the middle (0: as they are)"));
    spreadForm->addRow(tr("Pull apart:"), m_spread);
    left->addLayout(spreadForm);

    m_inside = new QCheckBox(tr("The parts inside joined shapes"));
    m_inside->setToolTip(tr("For a kit or a merged shape, number and show each part in it (as the Parts List does "
                            "when it lists them)"));
    m_inside->setChecked(prefs.value(QStringLiteral("parts/inside"), false).toBool());
    left->addWidget(m_inside);
    m_table = new QCheckBox(tr("A parts list under the picture"));
    m_table->setChecked(prefs.value(QStringLiteral("sheet/table"), true).toBool());
    left->addWidget(m_table);

    m_page = new QComboBox;
    m_page->addItem(tr("Letter"));
    m_page->addItem(tr("A4"));
    m_page->setCurrentIndex(prefs.value(QStringLiteral("sheet/page"), 0).toInt() == 1 ? 1 : 0);
    left->addWidget(labelled(tr("Paper:"), m_page));

    left->addWidget(new QLabel(tr("Notes for putting it together:")));
    m_notes = new QPlainTextEdit;
    m_notes->setPlaceholderText(tr("1. Push the head (2) onto the neck peg of the body (1).\n2. ..."));
    m_notes->setToolTip(tr("Printed under the parts list, as you write it (one paragraph per line)"));
    m_notes->setMinimumWidth(300);
    left->addWidget(m_notes, 1);

    m_status = new QLabel;
    m_status->setWordWrap(true);
    left->addWidget(m_status);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    auto* save = buttons->addButton(tr("Save as PDF..."), QDialogButtonBox::ActionRole);
    left->addWidget(buttons);

    m_preview = new QLabel;
    m_preview->setMinimumSize(560, 480);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setStyleSheet(QStringLiteral("background: white; border: 1px solid #c8ccd0;"));
    layout->addWidget(m_preview, 1);

    m_later = new QTimer(this);
    m_later->setSingleShot(true);
    m_later->setInterval(200);
    connect(m_later, &QTimer::timeout, this, &InstructionSheetDialog::redraw);
    connect(m_view, &QComboBox::currentIndexChanged, m_later, qOverload<>(&QTimer::start));
    connect(m_spread, &QDoubleSpinBox::valueChanged, this, [this] {
        rebuild();
        m_later->start();
    });
    connect(m_inside, &QCheckBox::toggled, this, [this] {
        rebuild();
        m_later->start();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, [this] {
        remember();
        reject();
    });
    connect(save, &QPushButton::clicked, this, [this] {
        const QString start = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QLatin1Char('/') +
                              m_title->text() + tr(" instructions.pdf");
        const QString path =
            QFileDialog::getSaveFileName(this, tr("Save Instruction Sheet"), start, tr("PDF (*.pdf)"));
        if (path.isEmpty())
            return;
        remember();
        const QString problem = savePdf(path);
        m_status->setText(problem.isEmpty() ? tr("Saved %1.").arg(QDir::toNativeSeparators(path))
                                            : tr("It couldn't be saved: %1").arg(problem));
    });
    rebuild();
}

void InstructionSheetDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    m_later->start();  // the preview fits the new size
}

void InstructionSheetDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    m_later->start();
}

void InstructionSheetDialog::remember() const
{
    QSettings prefs = preferences();
    prefs.setValue(QStringLiteral("sheet/view"), m_view->currentIndex());
    prefs.setValue(QStringLiteral("sheet/spread"), m_spread->value());
    prefs.setValue(QStringLiteral("sheet/table"), m_table->isChecked());
    prefs.setValue(QStringLiteral("sheet/page"), m_page->currentIndex());
    prefs.setValue(QStringLiteral("parts/inside"), m_inside->isChecked());
}

illustrate::View InstructionSheetDialog::view() const
{
    return illustrate::View(m_view->currentIndex());
}

void InstructionSheetDialog::rebuild()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    try {
        parts::Settings s;
        s.inside = m_inside->isChecked();
        m_rows = parts::rows(m_doc, m_ids, s);
        m_items = illustrate::exploded(m_doc, m_rows, m_spread->value() / 100.0);
        int numbered = 0;
        for (const auto& r : m_rows)
            numbered += r.number > 0;
        m_status->setText(numbered == 1 ? tr("1 part.") : tr("%1 parts, numbered as in the Parts List.").arg(numbered));
    } catch (const std::exception& e) {
        m_items.clear();
        m_status->setText(QString::fromUtf8(e.what()));
    }
    QApplication::restoreOverrideCursor();
}

void InstructionSheetDialog::redraw()
{
    const QSize size = m_preview->size() - QSize(4, 4);
    QImage canvas(size.expandedTo(QSize(64, 64)), QImage::Format_ARGB32);
    canvas.fill(Qt::white);
    try {
        const illustrate::Picture pic = illustrate::render(m_items, view(), canvas.width(), canvas.height());
        QPainter p(&canvas);
        paintIllustration(p, pic, QRectF(0, 0, canvas.width(), canvas.height()));
    } catch (const std::exception& e) {
        m_status->setText(QString::fromUtf8(e.what()));
    }
    m_preview->setPixmap(QPixmap::fromImage(canvas));
}

QString InstructionSheetDialog::savePdf(const QString& path)
{
    QPdfWriter pdf(path);
    pdf.setPageSize(QPageSize(m_page->currentIndex() == 1 ? QPageSize::A4 : QPageSize::Letter));
    pdf.setPageMargins(QMarginsF(12, 12, 12, 12), QPageLayout::Millimeter);
    pdf.setResolution(200);
    pdf.setTitle(m_title->text());
    pdf.setCreator(QStringLiteral("Forge3D"));
    QPainter p;
    if (!p.begin(&pdf))
        return tr("the file couldn't be written");
    QApplication::setOverrideCursor(Qt::WaitCursor);
    struct Restore {
        ~Restore() { QApplication::restoreOverrideCursor(); }
    } restore;
    const QRectF page(0, 0, pdf.width(), pdf.height());
    const double dpi = pdf.resolution();
    const auto pt = [&](double points) { return points * dpi / 72.0; };  // a font size in pixels
    double y = 0;
    const auto newPage = [&] {
        pdf.newPage();
        y = 0;
    };

    // The title, and what the picture shows.
    QFont titleFont = QApplication::font();
    titleFont.setPixelSize(int(pt(20)));
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.setPen(QColor(0x1f, 0x23, 0x27));
    const double titleH = QFontMetricsF(titleFont).height();
    p.drawText(QRectF(0, y, page.width(), titleH), Qt::AlignLeft | Qt::AlignVCenter, m_title->text());
    y += titleH;
    QFont small = QApplication::font();
    small.setPixelSize(int(pt(9.5)));
    p.setFont(small);
    p.setPen(QColor(0x60, 0x66, 0x6c));
    int numbered = 0;
    for (const auto& r : m_rows)
        numbered += r.number > 0;
    const double smallH = QFontMetricsF(small).height();
    p.drawText(QRectF(0, y, page.width(), smallH), Qt::AlignLeft | Qt::AlignVCenter,
               tr("%1 parts · exploded view: %2").arg(numbered).arg(tr(illustrate::title(view())).toLower()));
    y += smallH * 1.6;

    // The picture: as big as it can be, leaving room for a short parts list.
    const double pictureH = std::min(page.width() * 0.85, (page.height() - y) * (m_table->isChecked() ? 0.66 : 1.0));
    const QRectF area(0, y, page.width(), pictureH);
    try {
        // Drawn at about 1.4 pixels for each one printed.
        const double k = std::min(1.4, 2400.0 / std::max(area.width(), area.height()));
        const illustrate::Picture pic =
            illustrate::render(m_items, view(), int(area.width() * k), int(area.height() * k));
        paintIllustration(p, pic, area);
    } catch (const std::exception& e) {
        p.end();
        return QString::fromUtf8(e.what());
    }
    y += pictureH + smallH;

    // The parts list.
    QFont body = QApplication::font();
    body.setPixelSize(int(pt(10)));
    QFont bold = body;
    bold.setBold(true);
    const double rowH = QFontMetricsF(body).height() * 1.35;
    const double numberW = pt(28), sizeW = pt(130);
    const auto header = [&] {
        p.setFont(bold);
        p.setPen(QColor(0x1f, 0x23, 0x27));
        p.drawText(QRectF(0, y, numberW, rowH), Qt::AlignLeft | Qt::AlignVCenter, tr("#"));
        p.drawText(QRectF(numberW, y, page.width() - numberW - sizeW, rowH), Qt::AlignLeft | Qt::AlignVCenter,
                   tr("Part"));
        p.drawText(QRectF(page.width() - sizeW, y, sizeW, rowH), Qt::AlignRight | Qt::AlignVCenter, tr("Size"));
        y += rowH;
        p.setPen(QPen(QColor(0x9a, 0xa0, 0xa6), pt(0.6)));
        p.drawLine(QPointF(0, y), QPointF(page.width(), y));
    };
    if (m_table->isChecked()) {
        if (y + rowH * 3 > page.height())
            newPage();
        header();
        for (const parts::Row& r : m_rows) {
            if (y + rowH > page.height()) {
                newPage();
                header();
            }
            p.setFont(r.number > 0 ? body : bold);
            p.setPen(QColor(0x1f, 0x23, 0x27));
            const double indent = r.depth > 0 ? pt(14) : 0.0;
            if (r.number > 0)
                p.drawText(QRectF(0, y, numberW, rowH), Qt::AlignLeft | Qt::AlignVCenter, QString::number(r.number));
            p.drawText(QRectF(numberW + indent, y, page.width() - numberW - sizeW - indent, rowH),
                       Qt::AlignLeft | Qt::AlignVCenter, QString::fromStdString(r.name));
            p.setFont(body);
            p.drawText(QRectF(page.width() - sizeW, y, sizeW, rowH), Qt::AlignRight | Qt::AlignVCenter, sizeText(r));
            y += rowH;
        }
        y += rowH * 0.6;
    }

    // The notes, a paragraph at a time.
    const QString notes = m_notes->toPlainText().trimmed();
    if (!notes.isEmpty()) {
        QFont heading = bold;
        heading.setPixelSize(int(pt(12)));
        const double headingH = QFontMetricsF(heading).height() * 1.4;
        if (y + headingH + rowH * 2 > page.height())
            newPage();
        p.setFont(heading);
        p.setPen(QColor(0x1f, 0x23, 0x27));
        p.drawText(QRectF(0, y, page.width(), headingH), Qt::AlignLeft | Qt::AlignVCenter, tr("Putting it together"));
        y += headingH;
        p.setFont(body);
        for (const QString& para : notes.split(QLatin1Char('\n'))) {
            const QString text = para.trimmed();
            if (text.isEmpty()) {
                y += rowH * 0.5;
                continue;
            }
            const QRectF need =
                p.boundingRect(QRectF(0, 0, page.width(), 1e6), Qt::TextWordWrap | Qt::AlignLeft, text);
            if (y + need.height() > page.height())
                newPage();
            p.drawText(QRectF(0, y, page.width(), need.height()), Qt::TextWordWrap | Qt::AlignLeft, text);
            y += need.height() + rowH * 0.3;
        }
    }
    p.end();
    return {};
}

}  // namespace forge::app
