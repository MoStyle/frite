/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2018-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TOOLSMANAGER_H
#define TOOLSMANAGER_H

#include "basemanager.h"
#include "tools/tool.h"
#include <QPolygonF>

class Tool;

class ToolsManager : public BaseManager {
    Q_OBJECT
   public:
    ToolsManager(QObject *pParent);

    virtual ~ToolsManager();

    void initTools();

    Tool *currentTool() const { return m_currentTool; }

    Tool *previousTool() const { return m_previousTool; }

    Tool *tool(Tool::ToolType _tool) const { return m_tools[_tool]; }

    const Tool * const * const tools() const { return m_tools; }

    void changeTool(Tool::ToolType toolType);

    void setTool(Tool::ToolType toolType); // TODO: make private, friend mainwindow

    void restorePreviousTool();

signals:
    void penSelected();
    void drawEndKeyframeSelected();
    void eraserSelected();
    void handSelected();
    void selectSelected();
    void trajectorySelected();
    void drawTrajectorySelected();
    void tangentSelected();
    void lassoSelected();
    void scribbleSelected();
    void deformSelected();
    void warpSelected();
    void strokeDeformSelected();
    void registrationLassoSelected();
    void correspondenceSelected();
    void fillGridSelected();
    void directMatchingSelected();
    void moveFramesSelected();
    void halvesSelected();
    void simplifySpacingSelected();
    void toolChanged(Tool *newTool);

   private:
    void signalToWindow(Tool::ToolType toolType);

    Tool *m_currentTool, *m_previousTool;
    Tool *m_tools[Tool::NoTool];
};

#endif