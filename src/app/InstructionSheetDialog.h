// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Illustration.h"
#include "forge/core/PartsList.h"

#include <QDialog>

#include <vector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPainter;
class QPlainTextEdit;
class QTimer;

namespace forge {
class Document;
}

namespace forge::app {

// Instruction Sheet (slice 78): an exploded view of the shapes with numbered balloons, the
// parts list under it and notes for putting it together, saved as a PDF to go with a kit.
class InstructionSheetDialog : public QDialog {
    Q_OBJECT
public:
    InstructionSheetDialog(const Document& doc, std::vector<ObjectId> ids, const QString& title,
                           QWidget* parent = nullptr);

    // Writes the sheet to `path` (a PDF). Returns an error, or empty.
    QString savePdf(const QString& path);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void rebuild();   // the parts (and their exploded meshes) again
    void redraw();    // the preview
    void remember() const;
    illustrate::View view() const;

    const Document& m_doc;
    std::vector<ObjectId> m_ids;
    std::vector<parts::Row> m_rows;
    std::vector<illustrate::Item> m_items;
    QLineEdit* m_title = nullptr;
    QComboBox* m_view = nullptr;
    QDoubleSpinBox* m_spread = nullptr;
    QCheckBox* m_inside = nullptr;
    QCheckBox* m_table = nullptr;
    QComboBox* m_page = nullptr;
    QPlainTextEdit* m_notes = nullptr;
    QLabel* m_preview = nullptr;
    QLabel* m_status = nullptr;
    QTimer* m_later = nullptr;
};

// Draws a picture with its balloons into `target` (the picture's pixels scaled to fit).
void paintIllustration(QPainter& p, const illustrate::Picture& pic, const QRectF& target);

}  // namespace forge::app
