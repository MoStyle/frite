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

#ifndef COLORMANAGER_H
#define COLORMANAGER_H

#include <QColor>
#include "basemanager.h"

class ColorManager : public BaseManager
{
    Q_OBJECT
public:
    explicit ColorManager(Editor* editor);
    ~ColorManager() override;

    QColor frontColor();
    void setColor(const QColor& color);

Q_SIGNALS:
    void colorChanged(QColor); // new color

private:
    QColor mCurrentFrontColor{ 33, 33, 33, 255 };
};

#endif // COLORMANAGER_H
