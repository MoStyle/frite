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
    void maskPenSelected();
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
    void proxySpacingSelected();
    void movePartialsSelected();
    void groupOrderingSelected();
    void localMaskSelected();
    void debugSelected();
    void pivotEditSelected();
    void pivotCreationSelected();
    void pivotTangentSelected();
    void pivotRotationSelected();
    void pivotScalingSelected();
    void pivotTranslationSelected();
    void pickStrokesSelected();
    void visibilitySelected();
    void toolChanged(Tool *newTool);

   private:
    void signalToWindow(Tool::ToolType toolType);

    Tool *m_currentTool, *m_previousTool;
    Tool *m_tools[Tool::NoTool];
};

#endif