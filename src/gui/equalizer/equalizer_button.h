/*
 * SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_EQUALIZER_BUTTON_H
#define __KIS_EQUALIZER_BUTTON_H

#include <QScopedPointer>
#include <QAbstractButton>


class EqualizerButton : public QAbstractButton
{
public:
    EqualizerButton(QWidget *parent);
    ~EqualizerButton();

    void paintEvent(QPaintEvent *event);
    void setRightmost(bool value);

    QSize sizeHint() const;
    QSize minimumSizeHint() const;

    void enterEvent(QEvent *event);
    void leaveEvent(QEvent *event);

private:
    struct Private;
    const QScopedPointer<Private> m_d;
};

#endif /* __KIS_EQUALIZER_BUTTON_H */
