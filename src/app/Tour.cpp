// SPDX-License-Identifier: GPL-3.0-or-later
#include "Tour.h"

#include <QEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

namespace forge::app {

namespace {
constexpr int kPad = 6;       // room round the thing pointed at
constexpr int kGap = 14;      // between it and the note
constexpr int kNoteWidth = 340;
}  // namespace

Tour::Tour(QWidget* window, std::vector<Step> steps) : QWidget(window), m_window(window), m_steps(std::move(steps))
{
    // The dimming: over the whole window, letting clicks through to it.
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setGeometry(window->rect());

    m_note = new QFrame(window);
    m_note->setObjectName(QStringLiteral("TourNote"));
    m_note->setStyleSheet(QStringLiteral(
        "#TourNote { background: white; border: 2px solid #2b78d0; border-radius: 8px; }"
        "#TourNote QLabel { background: transparent; color: #2b2f33; }"));
    m_note->setFixedWidth(kNoteWidth);
    auto* shadow = new QGraphicsDropShadowEffect(m_note);
    shadow->setBlurRadius(18);
    shadow->setOffset(0, 3);
    shadow->setColor(QColor(0, 0, 0, 90));
    m_note->setGraphicsEffect(shadow);
    auto* layout = new QVBoxLayout(m_note);
    layout->setContentsMargins(14, 12, 14, 10);
    m_title = new QLabel;
    QFont bold = m_title->font();
    bold.setBold(true);
    bold.setPointSizeF(bold.pointSizeF() * 1.15);
    m_title->setFont(bold);
    m_title->setWordWrap(true);
    m_text = new QLabel;
    m_text->setWordWrap(true);
    m_text->setTextFormat(Qt::RichText);
    layout->addWidget(m_title);
    layout->addWidget(m_text);
    auto* buttons = new QHBoxLayout;
    m_count = new QLabel;
    m_count->setStyleSheet(QStringLiteral("color: #7a8088;"));
    m_skip = new QPushButton(tr("Skip tour"));
    m_skip->setFlat(true);
    m_back = new QPushButton(tr("Back"));
    m_next = new QPushButton(tr("Next"));
    m_next->setDefault(true);
    buttons->addWidget(m_count);
    buttons->addStretch(1);
    buttons->addWidget(m_skip);
    buttons->addWidget(m_back);
    buttons->addWidget(m_next);
    layout->addLayout(buttons);
    m_note->hide();

    connect(m_skip, &QPushButton::clicked, this, &Tour::finish);
    connect(m_back, &QPushButton::clicked, this, [this] { show(m_index - 1); });
    connect(m_next, &QPushButton::clicked, this, [this] {
        if (m_index + 1 >= int(m_steps.size()))
            finish();
        else
            show(m_index + 1);
    });
    window->installEventFilter(this);
}

void Tour::start()
{
    QWidget::show();
    raise();
    show(0);
}

void Tour::show(int index)
{
    if (index < 0 || index >= int(m_steps.size()))
        return;
    m_index = index;
    const Step& step = m_steps[std::size_t(index)];
    if (step.before)
        step.before();
    m_target = step.target ? step.target() : nullptr;
    m_title->setText(step.title);
    m_text->setText(step.text);
    m_count->setText(tr("%1 of %2").arg(index + 1).arg(m_steps.size()));
    m_back->setVisible(index > 0);
    const bool last = index + 1 == int(m_steps.size());
    m_next->setText(last ? tr("Finish") : tr("Next"));
    m_skip->setVisible(!last);
    m_note->adjustSize();
    m_note->show();
    m_note->raise();
    place();
    m_next->setFocus();
}

QRect Tour::targetRect() const
{
    if (!m_target || !m_target->isVisible())
        return {};
    const QPoint topLeft = m_target->mapTo(m_window, QPoint(0, 0));
    return QRect(topLeft, m_target->size()).adjusted(-kPad, -kPad, kPad, kPad);
}

void Tour::place()
{
    setGeometry(m_window->rect());
    raise();
    m_note->raise();
    const QRect area = m_window->rect().adjusted(8, 8, -8, -8);
    const QRect t = targetRect();
    const QSize n = m_note->sizeHint();
    QPoint at;
    if (t.isNull()) {
        at = area.center() - QPoint(n.width() / 2, n.height() / 2);
    } else if (t.bottom() + kGap + n.height() <= area.bottom()) {
        at = QPoint(t.center().x() - n.width() / 2, t.bottom() + kGap);  // below
    } else if (t.left() - kGap - n.width() >= area.left()) {
        at = QPoint(t.left() - kGap - n.width(), t.center().y() - n.height() / 2);  // left
    } else if (t.right() + kGap + n.width() <= area.right()) {
        at = QPoint(t.right() + kGap, t.center().y() - n.height() / 2);  // right
    } else {
        at = QPoint(t.center().x() - n.width() / 2, t.top() - kGap - n.height());  // above
    }
    at.setX(std::clamp(at.x(), area.left(), std::max(area.left(), area.right() - n.width())));
    at.setY(std::clamp(at.y(), area.top(), std::max(area.top(), area.bottom() - n.height())));
    m_note->setGeometry(QRect(at, n));
    update();
}

void Tour::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath dim;
    dim.addRect(rect());
    const QRect t = targetRect();
    if (!t.isNull()) {
        QPainterPath hole;
        hole.addRoundedRect(t, 6, 6);
        dim = dim.subtracted(hole);
    }
    p.fillPath(dim, QColor(20, 30, 45, 70));
    if (!t.isNull()) {
        p.setPen(QPen(QColor(0x2b, 0x78, 0xd0), 3));
        p.drawRoundedRect(t, 6, 6);
    }
}

bool Tour::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_window) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest)
            place();
        else if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape)
            finish();
    }
    return QWidget::eventFilter(watched, event);
}

void Tour::finish()
{
    m_window->removeEventFilter(this);
    m_note->hide();
    m_note->deleteLater();
    hide();
    emit finished();
    deleteLater();
}

}  // namespace forge::app
