/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __DRAWENDKEYFRAMETOOL_H__
#define __DRAWENDKEYFRAMETOOL_H__

#include "tool.h"
#include "dialsandknobs.h"
#include "stroke.h"

class QGraphicsItem;
class Stroke;

class DrawEndKeyframeTool : public Tool {
    Q_OBJECT
public:
    DrawEndKeyframeTool(QObject *parent, Editor *editor);
    virtual ~DrawEndKeyframeTool();

    Tool::ToolType toolType() const override;

    QGraphicsItem *graphicsItem() override;

    QCursor makeCursor(float scaling=1.0f) const override;

    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;

    QPen &pen() { return m_pen; }

    Stroke *currentStroke() const { return m_currentStroke.get(); }
    
private:
    QBrush m_brush;
    QPen m_pen;
    StrokePtr m_currentStroke;
    double m_startTime, m_curTime;
    bool m_pressed;
};

#endif // __DRAWENDKEYFRAMETOOL_H__