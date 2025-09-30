/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __REGISTRATIONLASSOTOOL_H__
#define __REGISTRATIONLASSOTOOL_H__

#include "tools/lassotool.h"

class RegistrationLassoTool : public LassoTool {
public:

    RegistrationLassoTool(QObject *parent, Editor *editor);
    virtual ~RegistrationLassoTool() = default;

    Tool::ToolType toolType() const override;

    void toggled(bool on) override;
    void released(const EventInfo& info) override;
    void doublepressed(const EventInfo& info) override;
    void drawGL(VectorKeyFrame *key, qreal alpha) override;

private:

};

#endif // __REGISTRATIONLASSOTOOL_H__