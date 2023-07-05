/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __RIGIDDEFORMTOOL_H__
#define __RIGIDDEFORMTOOL_H__

#include "warptool.h"

class Layer;

class RigidDeformTool : public WarpTool {
    Q_OBJECT
public:
    enum RigidDeformType { Translation, Rotation, Reflection };
    Q_ENUM(RigidDeformType)

    RigidDeformTool(QObject *parent, Editor *editor);
    virtual ~RigidDeformTool();

    Tool::ToolType toolType() const override;

    QGraphicsItem *graphicsItem() override;

    QCursor makeCursor(float scaling=1.0f) const override;
    
    void toggled(bool on) override;
    void pressed(const EventInfo& info) Q_DECL_OVERRIDE;
    void moved(const EventInfo& info) Q_DECL_OVERRIDE;
    void released(const EventInfo& info) Q_DECL_OVERRIDE;
    void doublepressed(const EventInfo& info) Q_DECL_OVERRIDE;
    void draw(QPainter &painter, VectorKeyFrame *key) override;

private:
    void deformSelection(const Point::Affine &transform, const EventInfo &info);

    RigidDeformType m_deformType;
    Point::VectorType m_centerOfMass;
};

#endif // __RIGIDDEFORMTOOL_H__