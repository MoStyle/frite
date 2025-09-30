/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __CHARTTOOL_H__
#define __CHARTTOOL_H__

#include "tool.h"
#include "chartitem.h"

class ChartTickItem;
class QGraphicsSceneMouseEvent;

class ChartTool : public Tool {
    Q_OBJECT
public:
    explicit ChartTool(QObject *parent, Editor *editor) : Tool(parent, editor) { m_chartTool = true; m_chartMode = ChartItem::GROUP; }

    virtual void toggled(bool on);
    virtual void tickPressed(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) { }
    virtual void tickMoved(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) { }
    virtual void tickReleased(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) { }
    virtual void tickDoubleClick(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) { }

protected:
    ChartItem::ChartMode m_chartMode;
};

#endif // __CHARTTOOL_H__