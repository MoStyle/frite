/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __PIVOTROTATIONTOOL_H__
#define __PIVOTROTATIONTOOL_H__

#include "tool.h"
#include "pivottoolabstract.h"
#include "dialsandknobs.h"

class Group; 


class PivotRotationTool : public PivotToolAbstract {
    Q_OBJECT
public:
    PivotRotationTool(QObject *parent, Editor *editor);
    virtual ~PivotRotationTool();

    typedef enum { ROTATION, CONTEXT_MENU } State;

    Tool::ToolType toolType() const override;

    QCursor makeCursor(float scaling=1.0f) const override;
    
    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;
    void drawUI(QPainter &painter, VectorKeyFrame *key) override;
    

private:
    State m_currentState;
    Point::Scalar m_angle;
    Point::VectorType m_currentPos, m_initialDir;
    bool m_pressed;

};

#endif // __PIVOTROTATIONTOOL_H__