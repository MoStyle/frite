/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __ERASERTOOL_H__
#define __ERASERTOOL_H__

#include "tool.h"

class EraserTool : public Tool {
    Q_OBJECT
public:
    EraserTool(QObject *parent, Editor *editor);

    Tool::ToolType toolType() const override;

    QGraphicsItem *graphicsItem() override;

    QCursor makeCursor(float scaling=1.0f) const override;

    void pressed(const EventInfo& info) Q_DECL_OVERRIDE;
    void moved(const EventInfo& info) Q_DECL_OVERRIDE;
    void released(const EventInfo& info) Q_DECL_OVERRIDE;

private:
    void erase(const EventInfo& info);

    int m_prevFrame;
    VectorKeyFrame *m_keyframe;
};

#endif // __ERASERTOOL_H__