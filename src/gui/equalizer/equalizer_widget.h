/*
 * SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_EQUALIZER_WIDGET_H
#define __KIS_EQUALIZER_WIDGET_H

#include <QScopedPointer>
#include <QWidget>
#include <QMap>

#include "editor.h"

class EqualizerWidget : public QWidget
{
    Q_OBJECT

public:
    EqualizerWidget(int maxDistance, QWidget *parent);
    ~EqualizerWidget();

    EqualizerValues getValues() const;
    void setValues(const EqualizerValues &values);

    void toggleMasterSwitch();

    void resizeEvent(QResizeEvent *event);

    void mouseMoveEvent(QMouseEvent *ev);

signals:
    void sigConfigChanged();

private Q_SLOTS:
    void slotMasterColumnChanged(int, bool, int);

private:
    struct Private;
    const QScopedPointer<Private> m_d;
};

#endif /* __KIS_EQUALIZER_WIDGET_H */
