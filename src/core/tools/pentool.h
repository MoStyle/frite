/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __PENTOOL_H__
#define __PENTOOL_H__

#include "tool.h"
#include "dialsandknobs.h"
#include "stroke.h"

class QGraphicsItem;
class Stroke;

class PenTool : public Tool {
    Q_OBJECT
public:
    PenTool(QObject *parent, Editor *editor);
    virtual ~PenTool();

    Tool::ToolType toolType() const override;

    QGraphicsItem *graphicsItem() override;

    QCursor makeCursor(float scaling=1.0f) const override;

    void pressed(const EventInfo& info) Q_DECL_OVERRIDE;
    void moved(const EventInfo& info) Q_DECL_OVERRIDE;
    void released(const EventInfo& info) Q_DECL_OVERRIDE;

    QPen &pen() { return m_pen; }

    Stroke *currentStroke() const { return m_currentStroke.get(); }

private:
    QBrush m_brush;
    QPen m_pen;
    StrokePtr m_currentStroke;
    double m_startTime, m_curTime;
    bool m_pressed;
};

#endif // __PENTOOL_H__