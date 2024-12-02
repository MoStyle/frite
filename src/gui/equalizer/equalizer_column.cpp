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

#include "equalizer_column.h"

#include <QVBoxLayout>
#include <QFontMetrics>
#include <QApplication>

#include "equalizer_slider.h"
#include "equalizer_button.h"

struct EqualizerColumn::Private
{
    EqualizerButton *stateButton;
    EqualizerSlider *mainSlider;
    int id;
    bool forceDisabled;
};


EqualizerColumn::EqualizerColumn(QWidget *parent, int id, const QString &title)
    : QWidget(parent),
      m_d(new Private)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_d->id = id;

    m_d->stateButton = new EqualizerButton(this);
    m_d->stateButton->setText(title);
    m_d->stateButton->setCheckable(true);

    m_d->mainSlider = new EqualizerSlider(this);
    m_d->mainSlider->setRange(0, 100);
    m_d->mainSlider->setSingleStep(5);
    m_d->mainSlider->setPageStep(10);

    m_d->forceDisabled = false;

    QVBoxLayout *vbox = new QVBoxLayout(this);

    vbox->setSpacing(0);
    vbox->setContentsMargins(0,0,0,0);

    vbox->addWidget(m_d->stateButton);
    vbox->addWidget(m_d->mainSlider);

    setLayout(vbox);

    connect(m_d->stateButton, SIGNAL(toggled(bool)),
            SLOT(slotButtonChanged(bool)));

    connect(m_d->mainSlider, SIGNAL(valueChanged(int)),
            SLOT(slotSliderChanged(int)));
}

EqualizerColumn::~EqualizerColumn()
{
}

void EqualizerColumn::setRightmost(bool value)
{
    m_d->stateButton->setRightmost(value);
    m_d->mainSlider->setRightmost(value);
}

void EqualizerColumn::slotSliderChanged(int value)
{
    QSignalBlocker b(m_d->stateButton);
    m_d->stateButton->setChecked(value > 0);
    updateState();

    emit sigColumnChanged(m_d->id, m_d->stateButton->isChecked(), m_d->mainSlider->value());
}

void EqualizerColumn::slotButtonChanged(bool value)
{
    Q_UNUSED(value);
    emit sigColumnChanged(m_d->id, m_d->stateButton->isChecked(), m_d->mainSlider->value());

    updateState();
}

int EqualizerColumn::value() const
{
    return m_d->mainSlider->value();
}

void EqualizerColumn::setValue(int value)
{
    m_d->mainSlider->setValue(value);
}

bool EqualizerColumn::state() const
{
    return m_d->stateButton->isChecked();
}

void EqualizerColumn::setState(bool value)
{
    m_d->stateButton->setChecked(value);
}

void EqualizerColumn::setForceDisabled(bool value)
{
    m_d->forceDisabled = value;
    updateState();
}

void EqualizerColumn::updateState()
{
    bool showEnabled = m_d->stateButton->isChecked() && !m_d->forceDisabled;
    m_d->mainSlider->setToggleState(showEnabled);
}
