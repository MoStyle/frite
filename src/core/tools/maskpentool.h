/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __MAKSPENTOOL_H__
#define __MAKSPENTOOL_H__

#include "pentool.h"
#include "dialsandknobs.h"
#include "stroke.h"

class QGraphicsItem;
class Stroke;

class MaskPenTool : public PenTool {
    Q_OBJECT
public:
    MaskPenTool(QObject *parent, Editor *editor);
    virtual ~MaskPenTool();

    Tool::ToolType toolType() const override;

    void toggled(bool on) override;

    virtual void pressed(const EventInfo& info) Q_DECL_OVERRIDE;
    virtual void released(const EventInfo& info) Q_DECL_OVERRIDE;
private:
};

#endif // __MAKSPENTOOL_H__