/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __SPACINGPROXYTOOL_H__
#define __SPACINGPROXYTOOL_H__

#include "tool.h"
#include "charttool.h"

class SpacingProxyTool : public ChartTool {
    Q_OBJECT
public:
    SpacingProxyTool(QObject *parent, Editor *editor);
    virtual ~SpacingProxyTool();

    virtual Tool::ToolType toolType() const override;

    virtual QCursor makeCursor(float scaling=1.0f) const override;

    virtual void tickPressed(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    virtual void tickMoved(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    virtual void tickReleased(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    virtual void tickDoubleClick(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;

private:
};

#endif // __SPACINGPROXYTOOL_H__