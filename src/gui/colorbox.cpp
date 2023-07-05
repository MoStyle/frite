/*
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QVBoxLayout>
#include "colorwheel.h"
#include "colorbox.h"
#include "editor.h"

ColorBox::ColorBox( QWidget* parent ) : QDockWidget( parent )
{
    setWindowTitle(tr("Color Box", "Color Box window title"));
    mColorWheel = new ColorWheel(this);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->setContentsMargins(5, 5, 5, 5);
    layout->addWidget(mColorWheel);
    layout->setStretch(0, 1);
    layout->setStretch(1, 0);
    QWidget* mainWidget = new QWidget;
    mainWidget->setLayout(layout);
    setWidget(mainWidget);

    connect(mColorWheel, &ColorWheel::colorChanged, this, &ColorBox::onWheelMove);
    connect(mColorWheel, &ColorWheel::colorSelected, this, &ColorBox::onWheelRelease);
}

ColorBox::~ColorBox()
{
}

QColor ColorBox::color()
{
    return mColorWheel->color();
}

void ColorBox::setColor(QColor newColor)
{
    newColor = newColor.toHsv();

    if ( newColor != mColorWheel->color() )
    {
        mColorWheel->setColor(newColor);
    }
}

void ColorBox::onWheelMove(const QColor& color)
{
    emit colorChanged(color);
}

void ColorBox::onWheelRelease(const QColor& color)
{
     emit colorChanged(color);
}
