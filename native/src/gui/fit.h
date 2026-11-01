#pragma once
#include <QWidget>
#include <QCheckBox>
#include <QLabel>
#include <QComboBox>
#include <QList>

// Small layout helpers for the WinForms-ported fixed-geometry pages:
// WinForms (MS Sans Serif 8.25pt) text is narrower than Qt/Breeze rendering,
// so fixed pixel widths clip. These let controls grow to their content while
// keeping the original row start and spacing.
namespace Fit {

// Grow a widget horizontally to fit its content (keeps position/height).
inline void fit(QWidget *w)
{
    const QSize s = w->sizeHint();
    if (s.width() > w->width()) {
        w->resize(s.width(), w->height());
    }
    w->setMinimumWidth(qMax(w->width(), s.width()));
}

// Lay out widgets left-to-right starting at the first widget's x position,
// each sized to its content, with a fixed gap. Vertical positions are kept.
inline void row(const QList<QWidget *> &widgets, int gap = 10)
{
    if (widgets.isEmpty()) {
        return;
    }
    int x = widgets.first()->x();
    for (QWidget *w : widgets) {
        const QSize s = w->sizeHint();
        const int width = qMax(w->minimumWidth(), qMax(w->width(), s.width()));
        w->setGeometry(x, w->y(), width, qMax(w->height(), s.height()));
        w->setMinimumWidth(width);
        x += width + gap;
    }
}

// Fit a label and push the widget that follows it to the label's right edge.
inline void labelThen(QWidget *label, QWidget *next, int gap = 8, int dy = 0)
{
    fit(label);
    next->move(label->geometry().right() + 1 + gap, next->y() + dy);
}

} // namespace Fit
