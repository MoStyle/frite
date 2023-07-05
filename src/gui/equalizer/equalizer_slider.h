/*
 * SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_EQUALIZER_SLIDER_H
#define __KIS_EQUALIZER_SLIDER_H

#include <QScopedPointer>
#include <QAbstractSlider>

class EqualizerSlider : public QAbstractSlider
{
public:
    EqualizerSlider(QWidget *parent);
    ~EqualizerSlider();

    void mousePressEvent(QMouseEvent *ev);
    void mouseMoveEvent(QMouseEvent *ev);
    void mouseReleaseEvent(QMouseEvent *ev);
    void paintEvent(QPaintEvent *event);

    QSize sizeHint() const;
    QSize minimumSizeHint() const;

    void setRightmost(bool value);
    void setToggleState(bool value);

private:
    struct Private;
    const QScopedPointer<Private> m_d;
};

#endif /* __KIS_EQUALIZER_SLIDER_H */
