/*
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef COLORBOX_H
#define COLORBOX_H

#include <QDockWidget>

class ColorWheel;

class ColorBox : public QDockWidget
{
    Q_OBJECT

public:
    explicit ColorBox( QWidget* parent );
    virtual ~ColorBox() override;

    QColor color();
    void setColor(QColor);

Q_SIGNALS:
    void colorChanged(const QColor&);

private:
    void onWheelMove(const QColor&);
    void onWheelRelease(const QColor&);

    ColorWheel* mColorWheel = nullptr;
};

#endif // COLORBOX_H
