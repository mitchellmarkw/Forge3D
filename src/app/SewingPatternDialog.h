// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Sewing.h"

#include <QDialog>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>

#include <vector>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPainter;

namespace forge {
class Document;
}

namespace forge::app {

// Sewing Pattern (slice 79): the area of a shape painted one colour, cut into pieces and
// laid flat at full size with a seam allowance, saved as a PDF to print or an SVG for a
// cutting machine.
class SewingPatternDialog : public QDialog {
    Q_OBJECT
public:
    SewingPatternDialog(const Document& doc, ObjectId shape, QWidget* parent = nullptr);

    // Writes the pattern at full size. Returns an error, or empty.
    QString savePdf(const QString& path);
    QString saveSvg(const QString& path);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void rebuild();
    void redraw();
    void remember() const;
    // Where each piece goes (mm, y down), the sheet `width` mm wide; and the whole size.
    struct Layout {
        std::vector<QPointF> at;  // each piece's top left (its cut line's)
        QRectF square;            // the 50 mm check square
        double width = 0, height = 0;
    };
    Layout layOut(double width) const;
    // Draws the pattern in mm (the painter set up for mm).
    void paint(QPainter& p, const Layout& layout) const;
    double pageWidthMm() const;
    double pageHeightMm() const;

    const Document& m_doc;
    ObjectId m_shape;
    std::vector<std::uint32_t> m_colours;
    sewing::Pattern m_pattern;
    QString m_name;
    QComboBox* m_colour = nullptr;
    QComboBox* m_split = nullptr;
    QDoubleSpinBox* m_allowance = nullptr;
    QComboBox* m_page = nullptr;
    QLabel* m_preview = nullptr;
    QLabel* m_status = nullptr;
    bool m_started = false;
    // The cutting lines, kept for the allowance they were made with.
    mutable std::vector<QPainterPath> m_cuts;
    mutable double m_cutsFor = -1.0;
};

}  // namespace forge::app
