/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __PIVOTTANGENTTOOL_H__
#define __PIVOTTANGENTTOOL_H__

#include "tool.h"
#include "pivottoolabstract.h"

class Group; 


class PivotTangentTool : public PivotToolAbstract {
    Q_OBJECT
public:
    PivotTangentTool(QObject *parent, Editor *editor);
    virtual ~PivotTangentTool();

    Tool::ToolType toolType() const override;

    QCursor makeCursor(float scaling=1.0f) const override;
    
    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;
    void drawUI(QPainter &painter, VectorKeyFrame *key) override;
    

private:
    bool m_p1Pressed, m_p2Pressed;

};

#endif // __PIVOTTANGENTTOOL_H__