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

#include "equalizer_button.h"

#include <QStyle>
#include <QPainter>
#include <QStyleOption>
#include <QApplication>

struct EqualizerButton::Private
{
    Private(EqualizerButton *_q)
        : q(_q),
          isRightmost(false),
          isHovering(false) {}

    QRect boundingRect() const;
    QRect fillingRect() const;

    EqualizerButton *q;
    bool isRightmost;
    bool isHovering;
};

EqualizerButton::EqualizerButton(QWidget *parent)
    : QAbstractButton(parent),
      m_d(new Private(this))
{
    setFocusPolicy(Qt::ClickFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

EqualizerButton::~EqualizerButton()
{
}

void EqualizerButton::setRightmost(bool value)
{
    m_d->isRightmost = value;
}

QRect EqualizerButton::Private::boundingRect() const
{
    QRect bounds = q->rect().adjusted(0, 0, -isRightmost, 0);
    return bounds;
}

QRect EqualizerButton::Private::fillingRect() const
{
    const int offset = 3;
    QRect filling = boundingRect().adjusted(offset, offset, -offset, -offset);
    return filling;
}

void EqualizerButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    const QRect bounds = m_d->boundingRect();
    const QRect filling = m_d->fillingRect();
    const QColor backgroundColor = palette().color(QPalette::Base);

    QPainter p(this);

//    { // draw border
//        const QColor gridColor = QGuiApplication::palette().color(QPalette::Midlight);
//        const QPen gridPen(gridColor);

//        p.setPen(gridPen);
//        p.setBrush(backgroundColor);
//        p.drawRect(bounds);
//    }

    {
        QColor fillColor = QGuiApplication::palette().color(QPalette::Highlight);
        QColor frameColor = QGuiApplication::palette().color(QPalette::Highlight);

        if (isChecked() || hasFocus() || m_d->isHovering) {
            p.setPen(hasFocus() || m_d->isHovering ? frameColor : Qt::transparent);
            p.setBrush(isChecked() ? fillColor : Qt::transparent);
            p.drawRect(filling);
        }
    }

    QString textValue = text();

    { // draw text
        QColor textColor = isChecked() ? QGuiApplication::palette().color(QPalette::HighlightedText) : QGuiApplication::palette().color(QPalette::Text);
        int flags = Qt::AlignCenter | Qt::TextHideMnemonic;
        p.setPen(textColor);
        p.drawText(bounds, flags, textValue);
    }
}

QSize EqualizerButton::sizeHint() const
{
    QFontMetrics metrics(this->font());
    const int minHeight = metrics.height() + 10;
    return QSize(15, minHeight);
}

QSize EqualizerButton::minimumSizeHint() const
{
    QSize sh = sizeHint();
    return QSize(10, sh.height());
}

void EqualizerButton::enterEvent(QEvent *event)
{
    Q_UNUSED(event);

    m_d->isHovering = true;
    update();
}

void EqualizerButton::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);

    m_d->isHovering = false;
    update();
}
