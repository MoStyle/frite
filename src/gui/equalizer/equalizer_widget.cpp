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

#include "equalizer_widget.h"

#include <QMouseEvent>
#include <QApplication>
#include <QHBoxLayout>
#include <QMap>

#include "equalizer_column.h"
#include "editor.h"
#include "stylemanager.h"

struct EqualizerWidget::Private
{
    Private() : maxDistance(0)
    {
    }

    QMap<int, EqualizerColumn*> columns;
    int maxDistance;
};

EqualizerWidget::EqualizerWidget(int maxDistance, QWidget *parent)
    : QWidget(parent),
      m_d(new Private)
{
    m_d->maxDistance = maxDistance;

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setSpacing(1);

    for (int i = -m_d->maxDistance; i <= m_d->maxDistance; i++) {
        EqualizerColumn *c = new EqualizerColumn(this, i, QString::number(i));
        layout->addWidget(c);
        if(i == -1 || i == 0)
            layout->addSpacing(4);

        if (i == m_d->maxDistance) {
            c->setRightmost(true);
        }

        m_d->columns.insert(i, c);
        connect(c, SIGNAL(sigColumnChanged(int, bool, int)), this, SIGNAL(sigConfigChanged()));
    }
    connect(m_d->columns[0], SIGNAL(sigColumnChanged(int,bool,int)), this, SLOT(slotMasterColumnChanged(int, bool, int)));

    setLayout(layout);
}

EqualizerWidget::~EqualizerWidget()
{
}

EqualizerValues EqualizerWidget::getValues() const
{
    EqualizerValues v;
    v.maxDistance = m_d->maxDistance;

    for (int i = -m_d->maxDistance; i <= m_d->maxDistance; i++) {
        v.value.insert(i, m_d->columns[i]->value());
        v.state.insert(i, m_d->columns[i]->state());
    }

    return v;
}

void EqualizerWidget::setValues(const EqualizerValues &v)
{
    for (int i = -m_d->maxDistance; i <= m_d->maxDistance; i++) {
        if (qAbs(i) <= v.maxDistance) {
            m_d->columns[i]->setValue(v.value[i]);
            m_d->columns[i]->setState(v.state[i]);
        } else {
            m_d->columns[i]->setState(false);
        }
    }
}

void EqualizerWidget::toggleMasterSwitch()
{
    const bool currentState = m_d->columns[0]->state();
    m_d->columns[0]->setState(!currentState);
}

void EqualizerWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    const QSize newSize = m_d->columns[1]->size();

    QFont font = QApplication::font();
    while(font.pointSize() > 8) {
        QFontMetrics fm(font);

        QRect rc = fm.boundingRect(QString::number(100));

        if (rc.width() > newSize.width() ||
            rc.height() > newSize.height()) {

            font.setPointSize(font.pointSize() - 1);
        } else {
            break;
        }
    }

    if (font.pointSize() != this->font().pointSize()) {
        setFont(font);

        for (int i = -m_d->maxDistance; i <= m_d->maxDistance; i++) {
            m_d->columns[i]->setFont(font);
        }

    }
}

void EqualizerWidget::mouseMoveEvent(QMouseEvent *ev)
{
    if (!(ev->modifiers() & Qt::ShiftModifier)) return;

    QPointF globalPos = ev->globalPosition();
    QWidget *w = qApp->widgetAt(globalPos.toPoint());

    if (w && w->inherits("QAbstractSlider")) {
        QMouseEvent newEv(ev->type(),
                          w->mapFromGlobal(globalPos),
                          globalPos,
                          ev->button(),
                          ev->buttons(),
                          ev->modifiers() & ~Qt::ShiftModifier);
        qApp->sendEvent(w, &newEv);
    }
}

void EqualizerWidget::slotMasterColumnChanged(int, bool state, int)
{
    for (int i = 1; i <= m_d->maxDistance; i++) {
        m_d->columns[i]->setForceDisabled(!state);
        m_d->columns[-i]->setForceDisabled(!state);
    }
}
