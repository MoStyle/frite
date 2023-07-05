/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __MOVEFRAMESTOOL_H__
#define __MOVEFRAMESTOOL_H__

#include "tool.h"
#include "spacingtool.h"

class MoveFramesTool : public SpacingTool {
    Q_OBJECT
public:
    MoveFramesTool(QObject *parent, Editor *editor);
    virtual ~MoveFramesTool();

    virtual Tool::ToolType toolType() const override;

    virtual QGraphicsItem *graphicsItem() override;

    virtual QCursor makeCursor(float scaling=1.0f) const override;

    virtual void tickPressed(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    virtual void tickMoved(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    virtual void tickReleased(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    virtual void tickDoubleClick(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;

private:
    std::vector<float> m_offsetLeft, m_offsetRight;
};

#endif // __MOVEFRAMESTOOL_H__