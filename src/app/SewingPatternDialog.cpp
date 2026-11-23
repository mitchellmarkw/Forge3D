// SPDX-License-Identifier: GPL-3.0-or-later
#include "SewingPatternDialog.h"

#include "forge/core/Document.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPageLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPdfWriter>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QSvgGenerator>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace forge::app {

namespace {

constexpr double kGap = 8.0;      // mm between pieces
constexpr double kHeader = 14.0;  // mm at the top for the title
constexpr double kMargin = 10.0;  // mm round the printed page

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

// A piece's stitching line as a path (y down, the piece's top left at 0,0).
QPainterPath stitchPath(const sewing::Piece& piece)
{
    QPainterPath path;
    path.setFillRule(Qt::OddEvenFill);
    for (const auto& loop : piece.outline) {
        if (loop.size() < 3)
            continue;
        path.moveTo(loop[0][0], piece.height - loop[0][1]);
        for (std::size_t i = 1; i < loop.size(); ++i)
            path.lineTo(loop[i][0], piece.height - loop[i][1]);
        path.closeSubpath();
    }
    return path;
}

// The cutting line: the stitching line with the allowance all round.
QPainterPath cutPath(const sewing::Piece& piece, double allowance)
{
    const QPainterPath stitch = stitchPath(piece);
    if (allowance <= 0.0)
        return stitch;
    QPainterPathStroker stroker;
    stroker.setWidth(2.0 * allowance);
    stroker.setJoinStyle(Qt::RoundJoin);
    stroker.setCapStyle(Qt::RoundCap);
    return stitch.united(stroker.createStroke(stitch)).simplified();
}

QString colourName(std::uint32_t c)
{
    return QStringLiteral("#%1").arg(c & 0xFFFFFFu, 6, 16, QLatin1Char('0')).toUpper();
}

}  // namespace

SewingPatternDialog::SewingPatternDialog(const Document& doc, ObjectId shape, QWidget* parent)
    : QDialog(parent), m_doc(doc), m_shape(shape)
{
    setWindowTitle(tr("Sewing Pattern"));
    resize(980, 640);
    m_name = doc.find(shape) ? QString::fromStdString(doc.find(shape)->name) : QString();
    auto* layout = new QHBoxLayout(this);
    auto* left = new QVBoxLayout;
    layout->addLayout(left, 0);
    const QSettings prefs = preferences();

    auto* intro = new QLabel(tr("The area of %1 painted one colour (a jacket, a cape), cut into pieces and laid flat "
                                "at full size, to cut cloth from.")
                                 .arg(m_name));
    intro->setWordWrap(true);
    intro->setMaximumWidth(320);
    left->addWidget(intro);

    m_colour = new QComboBox;
    m_colours = sewing::paintedColours(doc, shape);
    for (std::uint32_t c : m_colours) {
        QPixmap swatch(16, 16);
        swatch.fill(QColor::fromRgb(c & 0xFFFFFFu));
        m_colour->addItem(QIcon(swatch), colourName(c));
    }
    m_colour->setToolTip(tr("The colour painted where the clothes go"));
    left->addWidget(labelled(tr("Painted colour:"), m_colour));

    m_split = new QComboBox;
    for (int i = 0; i < sewing::kSplitCount; ++i)
        m_split->addItem(tr(sewing::title(sewing::Split(i))));
    m_split->setCurrentIndex(std::clamp(prefs.value(QStringLiteral("sewing/split"), 0).toInt(), 0, sewing::kSplitCount - 1));
    m_split->setToolTip(tr("Where it's cut into pieces before they're laid flat (where the seams go). Anything that "
                           "goes all the way round needs a split."));
    left->addWidget(labelled(tr("Seams:"), m_split));

    auto* form = new QFormLayout;
    m_allowance = new QDoubleSpinBox;
    m_allowance->setRange(0.0, 25.0);
    m_allowance->setDecimals(1);
    m_allowance->setSingleStep(0.5);
    m_allowance->setSuffix(tr(" mm"));
    m_allowance->setValue(prefs.value(QStringLiteral("sewing/allowance"), 3.0).toDouble());
    m_allowance->setToolTip(tr("Extra cloth all round each piece, outside the stitching line (dashed), to sew the "
                               "seams with"));
    form->addRow(tr("Seam allowance:"), m_allowance);
    left->addLayout(form);

    m_page = new QComboBox;
    m_page->addItem(tr("Letter"));
    m_page->addItem(tr("A4"));
    m_page->setCurrentIndex(prefs.value(QStringLiteral("sheet/page"), 0).toInt() == 1 ? 1 : 0);
    left->addWidget(labelled(tr("Paper:"), m_page));

    m_status = new QLabel;
    m_status->setWordWrap(true);
    m_status->setMaximumWidth(320);
    left->addWidget(m_status);
    left->addStretch(1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    auto* pdf = buttons->addButton(tr("Save as PDF..."), QDialogButtonBox::ActionRole);
    pdf->setToolTip(tr("At full size, over as many pages as it needs; print at 100% (Actual size)"));
    auto* svg = buttons->addButton(tr("Save as SVG..."), QDialogButtonBox::ActionRole);
    svg->setToolTip(tr("At full size, in millimetres: for a cutting machine (Cricut, Silhouette) or to edit"));
    left->addWidget(buttons);

    m_preview = new QLabel;
    m_preview->setMinimumSize(560, 480);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setStyleSheet(QStringLiteral("background: white; border: 1px solid #c8ccd0;"));
    layout->addWidget(m_preview, 1);

    connect(m_colour, &QComboBox::currentIndexChanged, this, [this] {
        rebuild();
        redraw();
    });
    connect(m_split, &QComboBox::currentIndexChanged, this, [this] {
        rebuild();
        redraw();
    });
    connect(m_allowance, &QDoubleSpinBox::valueChanged, this, &SewingPatternDialog::redraw);
    connect(m_page, &QComboBox::currentIndexChanged, this, &SewingPatternDialog::redraw);
    connect(buttons, &QDialogButtonBox::rejected, this, [this] {
        remember();
        reject();
    });
    const auto save = [this](bool asPdf) {
        const QString start = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QLatin1Char('/') +
                              m_name + tr(" pattern") + (asPdf ? QStringLiteral(".pdf") : QStringLiteral(".svg"));
        const QString path = QFileDialog::getSaveFileName(this, tr("Save Sewing Pattern"), start,
                                                          asPdf ? tr("PDF (*.pdf)") : tr("SVG (*.svg)"));
        if (path.isEmpty())
            return;
        remember();
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const QString problem = asPdf ? savePdf(path) : saveSvg(path);
        QApplication::restoreOverrideCursor();
        m_status->setText(problem.isEmpty() ? tr("Saved %1.").arg(QDir::toNativeSeparators(path))
                                            : tr("It couldn't be saved: %1").arg(problem));
    };
    connect(pdf, &QPushButton::clicked, this, [save] { save(true); });
    connect(svg, &QPushButton::clicked, this, [save] { save(false); });
    m_status->setText(tr("Laying it flat..."));
}

void SewingPatternDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (m_started)
        return;
    m_started = true;
    // Shown first, then worked out (it takes a moment on a detailed figure).
    QTimer::singleShot(50, this, [this] {
        rebuild();
        redraw();
    });
}

void SewingPatternDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    redraw();
}

void SewingPatternDialog::remember() const
{
    QSettings prefs = preferences();
    prefs.setValue(QStringLiteral("sewing/split"), m_split->currentIndex());
    prefs.setValue(QStringLiteral("sewing/allowance"), m_allowance->value());
    prefs.setValue(QStringLiteral("sheet/page"), m_page->currentIndex());
}

double SewingPatternDialog::pageWidthMm() const
{
    return (m_page->currentIndex() == 1 ? 210.0 : 215.9) - 2 * kMargin;
}

double SewingPatternDialog::pageHeightMm() const
{
    return (m_page->currentIndex() == 1 ? 297.0 : 279.4) - 2 * kMargin;
}

void SewingPatternDialog::rebuild()
{
    m_pattern = {};
    m_cuts.clear();
    m_cutsFor = -1.0;
    const int i = m_colour->currentIndex();
    if (i < 0 || std::size_t(i) >= m_colours.size()) {
        m_status->setText(tr("%1 has no paint. Paint where the clothes go first (Sculpt → Paint), then come back.")
                              .arg(m_name));
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    try {
        m_pattern = sewing::fromPaint(m_doc, m_shape, m_colours[std::size_t(i)], sewing::Split(m_split->currentIndex()));
        double stretch = 0;
        for (const auto& p : m_pattern.pieces)
            stretch = std::max(stretch, p.stretch);
        QString text = m_pattern.pieces.size() == 1 ? tr("1 piece") : tr("%1 pieces").arg(m_pattern.pieces.size());
        if (!m_pattern.pieces.empty())
            text += tr("; laid flat, most lengths are within %1% of the figure's.").arg(stretch * 100.0, 0, 'f', 1);
        if (!m_pattern.problem.empty())
            text += QLatin1Char('\n') + QString::fromStdString(m_pattern.problem);
        m_status->setText(text);
    } catch (const std::exception& e) {
        m_status->setText(QString::fromUtf8(e.what()));
    }
    QApplication::restoreOverrideCursor();
}

SewingPatternDialog::Layout SewingPatternDialog::layOut(double width) const
{
    // Rows across the sheet, the check square first.
    Layout out;
    const double a = m_allowance->value();
    out.square = QRectF(0, kHeader, 50.0, 50.0);
    double x = 50.0 + kGap, y = kHeader, rowHeight = 50.0;
    for (const auto& piece : m_pattern.pieces) {
        const double w = piece.width + 2 * a, h = piece.height + 2 * a;
        if (x > 0 && x + w > width) {
            x = 0;
            y += rowHeight + kGap;
            rowHeight = 0;
        }
        out.at.emplace_back(x, y);
        x += w + kGap;
        rowHeight = std::max(rowHeight, h);
        out.width = std::max(out.width, x - kGap);
    }
    out.width = std::max(out.width, 50.0);
    out.height = y + rowHeight;
    return out;
}

void SewingPatternDialog::paint(QPainter& p, const Layout& layout) const
{
    const double a = m_allowance->value();
    const QColor ink(0x1f, 0x23, 0x27);
    p.setRenderHint(QPainter::Antialiasing);
    // The title.
    QFont font = QApplication::font();
    font.setPixelSize(5);  // mm (the painter works in mm)
    font.setBold(true);
    p.setFont(font);
    p.setPen(ink);
    p.drawText(QRectF(0, 0, 400, 7), Qt::AlignLeft | Qt::AlignVCenter,
               tr("Sewing pattern: %1 (%2)").arg(m_name, m_colour->currentText()));
    font.setBold(false);
    font.setPixelSize(3);
    p.setFont(font);
    p.drawText(QRectF(0, 7, 400, 5), Qt::AlignLeft | Qt::AlignVCenter,
               a > 0 ? tr("Cut on the solid line; sew on the dashed line (%1 mm seam allowance). Full size: print at "
                          "100%.")
                           .arg(a, 0, 'f', 1)
                     : tr("No seam allowance: the solid line is the stitching line. Full size: print at 100%."));
    // The check square.
    p.setPen(QPen(ink, 0.3));
    p.setBrush(Qt::NoBrush);
    p.drawRect(layout.square);
    p.drawText(layout.square, Qt::AlignCenter | Qt::TextWordWrap, tr("This square should\nmeasure 50 mm"));

    if (m_cutsFor != a || m_cuts.size() != m_pattern.pieces.size()) {
        m_cuts.clear();
        for (const auto& piece : m_pattern.pieces)
            m_cuts.push_back(cutPath(piece, a));
        m_cutsFor = a;
    }
    for (std::size_t i = 0; i < m_pattern.pieces.size(); ++i) {
        const auto& piece = m_pattern.pieces[i];
        p.save();
        p.translate(layout.at[i] + QPointF(a, a));
        // Cloth: a light fill inside the cutting line.
        const QPainterPath& cut = m_cuts[i];
        p.setBrush(QColor(0xf3, 0xf5, 0xf8));
        p.setPen(QPen(ink, 0.35));
        p.drawPath(cut);
        if (a > 0) {
            QPen dashed(QColor(0x4a, 0x6f, 0x9c), 0.25);
            dashed.setDashPattern({6, 4});
            p.setPen(dashed);
            p.setBrush(Qt::NoBrush);
            p.drawPath(stitchPath(piece));
        }
        // The grain: up the figure.
        const double cx = piece.width / 2.0, len = std::max(4.0, piece.height * 0.55);
        const double top = (piece.height - len) / 2.0, bottom = top + len, head = std::min(2.5, len * 0.15);
        p.setPen(QPen(ink, 0.25));
        p.drawLine(QPointF(cx, top), QPointF(cx, bottom));
        for (double end : {top, bottom}) {
            const double dir = end == top ? 1.0 : -1.0;
            p.drawLine(QPointF(cx, end), QPointF(cx - head * 0.6, end + dir * head));
            p.drawLine(QPointF(cx, end), QPointF(cx + head * 0.6, end + dir * head));
        }
        // Its name, sized to fit.
        QFont label = QApplication::font();
        const double size = std::clamp(std::min(piece.width, piece.height) * 0.11, 1.6, 5.0);
        label.setPixelSize(std::max(1, int(std::round(size))));
        label.setBold(true);
        p.setFont(label);
        p.setPen(ink);
        const QRectF nameBox(cx + 1.0, piece.height / 2.0 - size * 1.6, std::max(piece.width / 2.0 - 1.0, 1.0), size * 3.2);
        p.drawText(nameBox, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                   tr("%1\ncut 1").arg(QString::fromStdString(piece.name)));
        p.restore();
    }
}

void SewingPatternDialog::redraw()
{
    if (!m_preview)
        return;
    const QSize size = (m_preview->size() - QSize(4, 4)).expandedTo(QSize(64, 64));
    QImage canvas(size, QImage::Format_ARGB32);
    canvas.fill(Qt::white);
    if (!m_pattern.pieces.empty()) {
        const Layout layout = layOut(pageWidthMm());
        QPainter p(&canvas);
        const double k = std::min((size.width() - 16) / layout.width, (size.height() - 16) / layout.height);
        p.translate(8, 8);
        p.scale(k, k);
        paint(p, layout);
        // The page edges, faintly.
        QPen edge(QColor(0xb8, 0xbe, 0xc4), 0.4);
        edge.setDashPattern({3, 3});
        p.setPen(edge);
        for (double x = pageWidthMm(); x < layout.width; x += pageWidthMm())
            p.drawLine(QPointF(x, 0), QPointF(x, layout.height));
        for (double y = pageHeightMm(); y < layout.height; y += pageHeightMm())
            p.drawLine(QPointF(0, y), QPointF(layout.width, y));
    }
    m_preview->setPixmap(QPixmap::fromImage(canvas));
}

QString SewingPatternDialog::savePdf(const QString& path)
{
    if (m_pattern.pieces.empty())
        return tr("there's no pattern to save");
    const Layout layout = layOut(pageWidthMm());
    QPdfWriter pdf(path);
    pdf.setPageSize(QPageSize(m_page->currentIndex() == 1 ? QPageSize::A4 : QPageSize::Letter));
    pdf.setPageMargins(QMarginsF(kMargin, kMargin, kMargin, kMargin), QPageLayout::Millimeter);
    pdf.setResolution(600);
    pdf.setTitle(tr("Sewing pattern: %1").arg(m_name));
    pdf.setCreator(QStringLiteral("Forge3D"));
    QPainter p;
    if (!p.begin(&pdf))
        return tr("the file couldn't be written");
    const double perMm = pdf.resolution() / 25.4;
    const double pw = pageWidthMm(), ph = pageHeightMm();
    const int across = std::max(1, int(std::ceil(layout.width / pw - 1e-9)));
    const int down = std::max(1, int(std::ceil(layout.height / ph - 1e-9)));
    bool first = true;
    for (int row = 0; row < down; ++row)
        for (int col = 0; col < across; ++col) {
            if (!first)
                pdf.newPage();
            first = false;
            p.save();
            p.scale(perMm, perMm);
            p.setClipRect(QRectF(0, 0, pw, ph));
            p.translate(-col * pw, -row * ph);
            paint(p, layout);
            p.restore();
            if (across * down > 1) {
                // Which page, to tape them together in order.
                p.save();
                p.scale(perMm, perMm);
                QFont f = QApplication::font();
                f.setPixelSize(3);
                p.setFont(f);
                p.setPen(QColor(0x80, 0x86, 0x8c));
                p.drawText(QRectF(0, ph - 5, pw, 5), Qt::AlignRight | Qt::AlignVCenter,
                           tr("Row %1, page %2 of %3 across").arg(row + 1).arg(col + 1).arg(across));
                p.restore();
            }
        }
    p.end();
    return {};
}

QString SewingPatternDialog::saveSvg(const QString& path)
{
    if (m_pattern.pieces.empty())
        return tr("there's no pattern to save");
    // One sheet as wide as a page (pieces wider than that make it wider).
    const Layout layout = layOut(pageWidthMm());
    constexpr double perMm = 10.0;  // 254 dots per inch: 10 per mm
    QSvgGenerator svg;
    svg.setFileName(path);
    svg.setResolution(254);
    svg.setSize(QSize(int(std::ceil(layout.width * perMm)), int(std::ceil(layout.height * perMm))));
    svg.setViewBox(QRectF(0, 0, layout.width * perMm, layout.height * perMm));
    svg.setTitle(tr("Sewing pattern: %1").arg(m_name));
    svg.setDescription(tr("Made with Forge3D. Full size, in millimetres."));
    QPainter p;
    if (!p.begin(&svg))
        return tr("the file couldn't be written");
    p.scale(perMm, perMm);
    paint(p, layout);
    p.end();
    return {};
}

}  // namespace forge::app
