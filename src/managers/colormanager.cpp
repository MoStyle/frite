/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2018-2023 Pierre Benard <pierre.g.benard@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "colormanager.h"

#include "editor.h"


ColorManager::ColorManager(Editor* editor) : BaseManager(editor)
{
}

ColorManager::~ColorManager()
{
}

QColor ColorManager::frontColor()
{
    return mCurrentFrontColor;
}

void ColorManager::setColor(const QColor& newColor)
{
    if (mCurrentFrontColor != newColor)
    {
        mCurrentFrontColor = newColor;

        emit colorChanged(mCurrentFrontColor);
    }
}
