/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __PIVOTCREATIONTOOL_H__
#define __PIVOTCREATIONTOOL_H__

#include "tool.h"
#include "pivottoolabstract.h"


class PivotCreationTool : public PivotToolAbstract {
    Q_OBJECT
public:
    PivotCreationTool(QObject *parent, Editor *editor);
    virtual ~PivotCreationTool();

    typedef enum { DEFAULT, CONTEXT_MENU, EDIT_ROTATION, PIVOT_TRANSLATION } State;

    Tool::ToolType toolType() const override;

    QCursor makeCursor(float scaling = 1.0f) const override;

    void toggled(bool on) override;
    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;
    void drawUI(QPainter &painter, VectorKeyFrame * key) override;

private:
    void update();

    State m_currentState;

    Point::VectorType m_currentPos;
    Point::VectorType m_mouseTranslation;
    
    Point::Scalar m_angle;
    Point::VectorType m_initialDir;
    QVector<float> m_selectedAngles;
    QVector<VectorKeyFrame *> m_keyFrameSelected;
    bool m_translationDone;
    bool m_rotationDone;
    bool m_pressed;
};


#endif // __PIVOTCREATIONTOOL_H__