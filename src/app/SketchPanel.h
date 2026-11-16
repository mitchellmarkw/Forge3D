// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace forge::app {

// The sketch's side panel: how held it is, and what Finish makes of it (a pad pushed
// out, or a pocket cut in, and how far).
class SketchPanel : public QWidget {
    Q_OBJECT

public:
    explicit SketchPanel(QWidget* parent = nullptr);

    // What Finish makes. Pocket and Groove cut into the shape sketched on.
    enum class Make { Pad, Pocket, Revolve, Groove, Loft, Sweep };
    struct Options {
        Make make = Make::Pad;
        double length = 10.0;
        double angle = 360.0;
        bool otherWay = false;
        bool bothWays = false;
        bool throughAll = false;
        bool ruled = false;      // Loft: straight sides
        bool spiral = false;     // Sweep: round a spiral (else along a drawn path)
        double pitch = 5.0;      // spiral
        double turns = 3.0;
        bool leftHand = false;
    };
    // onFace: sketching on a shape's face, so it can cut in. editing: changing one made
    // before (it stays adding, or cutting).
    void start(const QString& title, bool onFace, bool editing, const Options& options);
    // Only placing points (a Hole's spots): no Make section.
    void startPositions(const QString& title, const QString& message);
    Make make() const;
    bool pocket() const { return make() == Make::Pocket || make() == Make::Groove; }
    bool revolve() const { return make() == Make::Revolve || make() == Make::Groove; }
    double length() const;
    double angle() const;
    bool otherWay() const;
    bool bothWays() const;
    bool throughAll() const;
    // Loft: which outline is open (0 = the first, at the sketch plane), of how many,
    // and its height; straight sides.
    void setLoftOutline(int index, int count, double height);
    double loftHeight() const;
    bool ruled() const;
    // Sweep: along a spiral (else a drawn path), its pitch, turns and hand; whether the
    // path (not the outline) is open.
    bool spiral() const;
    double pitch() const;
    double turns() const;
    bool leftHand() const;
    void setSweepStage(bool path);
    void setStatus(const QString& text, bool problem, bool held);
    void setMessage(const QString& text, bool problem);

signals:
    void finishRequested();
    void cancelRequested();
    // Loft: go to the outline below (-1) or above (+1; past the top adds one).
    void outlineRequested(int step);  // -1 below, 1 above (adding one at the top), 0 remove this one
    // Sweep: open the path (true) or the outline (false).
    void pathRequested(bool path);
    // What Make makes changed.
    void makeChanged();

private:
    void updateLabels();

    QLabel* m_title = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_message = nullptr;
    QLabel* m_makeLabel = nullptr;
    QComboBox* m_make = nullptr;
    QLabel* m_lengthLabel = nullptr;
    QDoubleSpinBox* m_length = nullptr;
    QLabel* m_angleLabel = nullptr;
    QDoubleSpinBox* m_angle = nullptr;
    QLabel* m_axisHelp = nullptr;
    QWidget* m_makeGroup = nullptr;
    QLabel* m_help = nullptr;
    QCheckBox* m_otherWay = nullptr;
    QCheckBox* m_bothWays = nullptr;
    QCheckBox* m_throughAll = nullptr;
    QPushButton* m_finish = nullptr;
    QWidget* m_loftGroup = nullptr;
    QLabel* m_loftOutline = nullptr;
    QLabel* m_loftHeightLabel = nullptr;
    QDoubleSpinBox* m_loftHeight = nullptr;
    QPushButton* m_below = nullptr;
    QPushButton* m_above = nullptr;
    QPushButton* m_removeOutline = nullptr;
    QCheckBox* m_ruled = nullptr;
    QWidget* m_sweepGroup = nullptr;
    QComboBox* m_along = nullptr;
    QWidget* m_spiralGroup = nullptr;
    QDoubleSpinBox* m_pitch = nullptr;
    QDoubleSpinBox* m_turns = nullptr;
    QCheckBox* m_leftHand = nullptr;
    QPushButton* m_pathButton = nullptr;
    QLabel* m_sweepHelp = nullptr;
    bool m_pathStage = false;
    int m_outline = 0, m_outlines = 1;
};

}  // namespace forge::app
