/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __WARPTOOL_H__
#define __WARPTOOL_H__

#include "tool.h"

class Group; 

class WarpTool : public Tool {
    Q_OBJECT
public:
    WarpTool(QObject *parent, Editor *editor);
    virtual ~WarpTool();

    Tool::ToolType toolType() const override;

    QGraphicsItem *graphicsItem() override;

    QCursor makeCursor(float scaling=1.0f) const override;
    
    void toggled(bool on) override;
    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;
    void doublepressed(const EventInfo& info) override;
    void wheel(const WheelEventInfo& info) override;
    void draw(QPainter &painter, VectorKeyFrame *key) override;

private:
    Point::VectorType m_pivot;
    Point::Affine m_inverseRigidGlobal;
    bool m_registerToNextKeyframe;
};

#endif // __WARPTOOL_H__