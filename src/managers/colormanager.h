/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2018-2023 Pierre Benard <pierre.g.benard@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
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
