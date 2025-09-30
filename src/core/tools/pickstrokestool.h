/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef PICKSTROKESTOOL_H
#define PICKSTROKESTOOL_H

#include "tool.h"
#include "picktool.h"
#include "group.h"
#include "editor.h"

class StrokeInterval;
class StrokeIntervals;
class Lattice;

class PickStrokesTool : public PickTool {
    Q_OBJECT
public:
    PickStrokesTool(QObject *parent, Editor *editor);
    virtual ~PickStrokesTool() { }

    Tool::ToolType toolType() const override;

    virtual void toggled(bool on) override;
    virtual void pressed(const EventInfo& info) override;
    virtual void moved(const EventInfo& info) override;
    virtual void released(const EventInfo& info) override;

protected:
    void setOnionDirection();

    QList<Point *> m_lassoSelectedPoints;
    EqualizerValues m_savedEqValues;
    EqualizedMode m_savedEqMode;
    std::shared_ptr<Layer> m_savedLayer;
    bool m_savedLayerOnionSkinStatus;
};

#endif