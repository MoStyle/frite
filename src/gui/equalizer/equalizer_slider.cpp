/*
 *  Copyright (c) 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "equalizer_slider.h"

#include <QStyle>
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QStyleOptionSlider>

struct EqualizerSlider::Private
{
    Private(EqualizerSlider *_q) : q(_q), isRightmost(false), toggleState(true) {}

    EqualizerSlider *q;
    bool isRightmost;
    bool toggleState;


    QRect boundingRect() const;
    QRect sliderRect() const;

    int mousePosToValue(const QPoint &pt, bool round) const;
};

EqualizerSlider::EqualizerSlider(QWidget *parent)
    : QAbstractSlider(parent),
      m_d(new Private(this))
{
    setOrientation(Qt::Vertical);
    setFocusPolicy(Qt::ClickFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

EqualizerSlider::~EqualizerSlider()
{
}

void EqualizerSlider::setRightmost(bool value)
{
    m_d->isRightmost = value;
}

void EqualizerSlider::setToggleState(bool value)
{
    m_d->toggleState = value;
    update();

}

QRect EqualizerSlider::Private::boundingRect() const
{
    QRect bounds = q->rect().adjusted(0, 0, -isRightmost, -1);
    return bounds;
}

QRect EqualizerSlider::Private::sliderRect() const
{
    const int offset = 3;
    QRect filling = boundingRect().adjusted(offset + 1, offset + 1,
                                            -offset, -offset);

    return filling;
}

int EqualizerSlider::Private::mousePosToValue(const QPoint &pt,  bool round) const
{
    const QRect areaRect = sliderRect();

    int rawValue = -pt.y() + (areaRect.top() + areaRect.height());
    int maxRawValue = areaRect.height();

    int value = QStyle::sliderValueFromPosition(q->minimum(), q->maximum(), rawValue, maxRawValue);

    if (round) {
        const int singleStep = q->singleStep();
        value = ((value + singleStep / 2) / singleStep) * singleStep;
    }

    return value;
}

void EqualizerSlider::mousePressEvent(QMouseEvent *ev)
{
    if (maximum() == minimum() || (ev->buttons() ^ ev->button())) {
        ev->ignore();
        return;
    }

    const bool precise = ev->modifiers() & Qt::ControlModifier ||
        ev->button() == Qt::RightButton;

    int value = m_d->mousePosToValue(ev->pos(), !precise);
    setSliderPosition(value);
    triggerAction(SliderMove);
    setRepeatAction(SliderNoAction);
}

void EqualizerSlider::mouseMoveEvent(QMouseEvent *ev)
{
    if (ev->modifiers() & Qt::ShiftModifier &&
        !rect().contains(ev->pos())) {

        ev->ignore();
        return;
    }

    const bool precise = ev->modifiers() & Qt::ControlModifier ||
        ev->buttons() & Qt::RightButton;

    int value = m_d->mousePosToValue(ev->pos(), !precise);
    setSliderPosition(value);
    triggerAction(SliderMove);
    setRepeatAction(SliderNoAction);
}

void EqualizerSlider::mouseReleaseEvent(QMouseEvent *ev)
{
    Q_UNUSED(ev);
}

QSize EqualizerSlider::sizeHint() const
{
    return QSize(25, 150);
}

QSize EqualizerSlider::minimumSizeHint() const
{
    return QSize(10, 40);
}

void EqualizerSlider::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    const QRect bounds = m_d->boundingRect();
    const QColor backgroundColor = palette().color(QPalette::Base);


    QPainter p(this);

//    { // draw background
//        const QColor gridColor = QGuiApplication::palette().color(QPalette::Midlight);

//        p.setPen(Qt::transparent);
//        p.setBrush(gridColor);
//        p.drawRect(bounds);
//    }

    { // draw slider
        QRect sliderRect = m_d->sliderRect();
        const int sliderPos =  QStyle::sliderPositionFromValue(minimum(), maximum(), value(), sliderRect.height()-5);
//        sliderRect.adjust(-2, sliderRect.height() - sliderPos, 0, 0);

        QColor color = m_d->toggleState ? QGuiApplication::palette().color(QPalette::Midlight) : QGuiApplication::palette().color(QPalette::Shadow);
        p.setBrush(color);

        p.drawRect(sliderRect);

        p.setPen(QGuiApplication::palette().color(QPalette::WindowText));
        p.setBrush(QGuiApplication::palette().color(QPalette::Light));
        sliderRect.adjust(-2, sliderRect.height() - sliderPos - 5, 2, -sliderPos);
        p.drawRect(sliderRect);
    }

    // draw focus rect
    if (hasFocus()) {
        QStyleOptionFocusRect fropt;
        fropt.initFrom(this);
        fropt.backgroundColor = backgroundColor;

        int dfw1 = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &fropt, this) + 1,
            dfw2 = dfw1 * 2;
        int offset = -dfw1 - dfw2;
        fropt.rect = bounds.adjusted(-offset, -offset, offset, offset);

        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &fropt, &p, this);
    }
}
