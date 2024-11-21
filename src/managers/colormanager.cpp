/*

Pencil - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2018 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

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
