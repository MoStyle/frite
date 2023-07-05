/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __SPACINGTOOL_H__
#define __SPACINGTOOL_H__

#include "tool.h"

class ChartTickItem;
class QGraphicsSceneMouseEvent;

class SpacingTool : public Tool {
    Q_OBJECT
public:
    explicit SpacingTool(QObject *parent, Editor *editor) : Tool(parent, editor) { m_spacingTool = true; }

    virtual void toggled(bool on);
    virtual void tickPressed(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) { }
    virtual void tickMoved(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) { }
    virtual void tickReleased(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) { }
    virtual void tickDoubleClick(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) { }

private:
};

#endif // __SPACINGTOOL_H__