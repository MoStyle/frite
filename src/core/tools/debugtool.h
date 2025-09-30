/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __DEBUGTOOL_H__
#define __DEBUGTOOL_H__

#include "tool.h"

class Group; 

class DebugTool : public Tool {
    Q_OBJECT
public:
    DebugTool(QObject *parent, Editor *editor);
    virtual ~DebugTool();

    Tool::ToolType toolType() const override;


    QCursor makeCursor(float scaling=1.0f) const override;
    
    void toggled(bool on) override;
    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;
    void doublepressed(const EventInfo& info) override;
    void wheel(const WheelEventInfo& info) override;
    void drawUI(QPainter &painter, VectorKeyFrame *key) override;

private:
    Point::VectorType m_p;
};

#endif // __DEBUGTOOL_H__