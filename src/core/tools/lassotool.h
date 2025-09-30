/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef LASSO_TOOL_H
#define LASSO_TOOL_H

#include "tool.h"
#include "picktool.h"
#include "group.h"

class StrokeInterval;
class StrokeIntervals;
class Lattice;

class LassoTool : public PickTool {
    Q_OBJECT
public:
    LassoTool(QObject *parent, Editor *editor);
    virtual ~LassoTool() { }

    Tool::ToolType toolType() const override;

    virtual void pressed(const EventInfo& info) Q_DECL_OVERRIDE;
    virtual void moved(const EventInfo& info) Q_DECL_OVERRIDE;
    virtual void released(const EventInfo& info) Q_DECL_OVERRIDE;

protected:
    void makeSelection(const EventInfo& info, GroupType type, VectorKeyFrame *prev, StrokeIntervals &selection);
    void cloneSelection(const EventInfo& info, StrokeIntervals &selection);

    QList<Point *> m_lassoSelectedPoints;
};

#endif