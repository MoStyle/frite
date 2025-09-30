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

#ifndef __HANDTOOL_H__
#define __HANDTOOL_H__

#include "tool.h"

class HandTool : public Tool {
    Q_OBJECT
public:
    HandTool(QObject *parent, Editor *editor);
    virtual ~HandTool();

    Tool::ToolType toolType() const override;

    QCursor makeCursor(float scaling=1.0f) const override;
    
    void pressed(const EventInfo& info) Q_DECL_OVERRIDE;
    void moved(const EventInfo& info) Q_DECL_OVERRIDE;
    void released(const EventInfo& info) Q_DECL_OVERRIDE;
    void doublepressed(const EventInfo& info) Q_DECL_OVERRIDE;
private:
    bool m_pressed;
};

#endif // __HANDTOOL_H__