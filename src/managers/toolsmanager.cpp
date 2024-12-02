#include "toolsmanager.h"
#include "canvascommands.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "tabletcanvas.h" 
#include "tools/pentool.h"
#include "tools/drawendkeyframetool.h"
#include "tools/lassotool.h"
#include "tools/erasertool.h"
#include "tools/handtool.h"
#include "tools/picktool.h"
#include "tools/rigiddeformtool.h"
#include "tools/warptool.h"
#include "tools/strokedeformtool.h"
#include "tools/registrationlassotool.h"
#include "tools/maskpentool.h"
#include "tools/correspondencetool.h"
#include "tools/trajectorytool.h"
#include "tools/drawtrajectorytool.h"
#include "tools/tangenttool.h"
#include "tools/fillgridtool.h"
#include "tools/directmatchingtool.h"
#include "tools/moveframestool.h"
#include "tools/halvestool.h"
#include "tools/spacingproxytool.h"
#include "tools/pivottool.h"
#include "tools/pivotcreationtool.h"
#include "tools/pivottangenttool.h"
#include "tools/pivotrotationtool.h"
#include "tools/pivotscalingtool.h"
#include "tools/pivottranslationtool.h"
#include "tools/grouporderingtool.h"
#include "tools/movepartialstool.h"
#include "tools/localmasktool.h"
#include "tools/pickstrokestool.h"
#include "tools/visibilitytool.h"
#include "tools/debugtool.h"

ToolsManager::ToolsManager(QObject *pParent) : BaseManager(pParent) {
    m_previousTool = nullptr;
    m_currentTool = nullptr;
}

ToolsManager::~ToolsManager() {
    // Tools are QObjects, no need to delete them
}

void ToolsManager::initTools() {
    m_tools[Tool::Pen]               = new PenTool(this, m_editor);
    m_tools[Tool::DrawEndKeyframe]   = new DrawEndKeyframeTool(this, m_editor);
    m_tools[Tool::Eraser]            = new EraserTool(this, m_editor);
    m_tools[Tool::Hand]              = new HandTool(this, m_editor);
    m_tools[Tool::Select]            = new PickTool(this, m_editor);
    m_tools[Tool::RigidDeform]       = new RigidDeformTool(this, m_editor);
    m_tools[Tool::Warp]              = new WarpTool(this, m_editor);
    m_tools[Tool::StrokeDeform]      = new StrokeDeformTool(this, m_editor);
    m_tools[Tool::RegistrationLasso] = new RegistrationLassoTool(this, m_editor);
    m_tools[Tool::MaskPen]           = new MaskPenTool(this, m_editor);
    m_tools[Tool::Traj]              = new TrajectoryTool(this, m_editor);
    m_tools[Tool::DrawTraj]          = new DrawTrajectoryTool(this, m_editor);
    m_tools[Tool::TrajTangent]       = new TangentTool(this, m_editor);
    m_tools[Tool::Lasso]             = new LassoTool(this, m_editor);
    m_tools[Tool::Correspondence]    = new CorrespondenceTool(this, m_editor);
    m_tools[Tool::FillGrid]          = new FillGridTool(this, m_editor);
    m_tools[Tool::DirectMatching]    = new DirectMatchingTool(this, m_editor);
    m_tools[Tool::PivotCreation]     = new PivotCreationTool(this, m_editor);
    m_tools[Tool::PivotEdit]         = new PivotEditTool(this, m_editor);
    m_tools[Tool::PivotTangent]      = new PivotTangentTool(this, m_editor);
    m_tools[Tool::PivotRotation]     = new PivotRotationTool(this, m_editor);
    m_tools[Tool::PivotScaling]      = new PivotScalingTool(this, m_editor);
    m_tools[Tool::PivotTranslation]  = new PivotTranslationTool(this, m_editor);
    m_tools[Tool::MoveFrames]        = new MoveFramesTool(this, m_editor);
    m_tools[Tool::Halves]            = new HalvesTool(this, m_editor);
    m_tools[Tool::SimplifySpacing]   = nullptr;
    m_tools[Tool::ProxySpacing]      = new SpacingProxyTool(this, m_editor);
    m_tools[Tool::MovePartials]      = new MovePartialsTool(this, m_editor);
    m_tools[Tool::GroupOrdering]     = new GroupOrderingTool(this, m_editor);
    m_tools[Tool::LocalMask]         = new LocalMaskTool(this, m_editor);
    m_tools[Tool::CopyStrokes]       = new PickStrokesTool(this, m_editor);
    m_tools[Tool::Visibility]        = new VisibilityTool(this, m_editor);
    m_tools[Tool::Debug]             = new DebugTool(this, m_editor);

    connect(m_tools[Tool::Pen], &Tool::updateFrame, m_editor->tabletCanvas(), &TabletCanvas::updateCurrentFrame);
    // connect(m_editor, &Editor::currentFrameChanged, m_tools[Tool::GroupOrdering], &GroupOrderingTool::frameChanged);
    connect(m_editor, SIGNAL(currentFrameChanged(int)), m_tools[Tool::GroupOrdering], SLOT(frameChanged(int)));


    setTool(Tool::Pen);
}

void ToolsManager::changeTool(Tool::ToolType toolType) {
    signalToWindow(toolType);
}

void ToolsManager::restorePreviousTool() {
    if (m_previousTool != nullptr) {
        setTool(m_previousTool->toolType());
    }
}

void ToolsManager::signalToWindow(Tool::ToolType toolType) {
    switch(toolType) {
        case Tool::Pen:                 emit penSelected(); break;
        case Tool::DrawEndKeyframe:     emit drawEndKeyframeSelected(); break;
        case Tool::Eraser:              emit eraserSelected(); break;
        case Tool::Hand:                emit handSelected(); break;
        case Tool::Select:              emit selectSelected(); break;
        case Tool::Traj:                emit trajectorySelected(); break;
        case Tool::DrawTraj:            emit drawTrajectorySelected(); break;
        case Tool::TrajTangent:         emit tangentSelected(); break;
        case Tool::Lasso:               emit lassoSelected(); break;
        case Tool::MaskPen:             emit maskPenSelected(); break;
        case Tool::RigidDeform:         emit deformSelected(); break;
        case Tool::Warp:                emit warpSelected(); break;
        case Tool::StrokeDeform:        emit strokeDeformSelected(); break;
        case Tool::RegistrationLasso:   emit registrationLassoSelected(); break;
        case Tool::Correspondence:      emit correspondenceSelected(); break;
        case Tool::FillGrid:            emit fillGridSelected(); break;
        case Tool::DirectMatching:      emit directMatchingSelected(); break;
        case Tool::PivotCreation:       emit pivotCreationSelected(); break;
        case Tool::PivotEdit:           emit pivotEditSelected(); break;
        case Tool::PivotTangent:        emit pivotTangentSelected(); break;
        case Tool::PivotRotation:       emit pivotRotationSelected(); break;
        case Tool::PivotScaling:        emit pivotScalingSelected(); break;
        case Tool::PivotTranslation:    emit pivotTranslationSelected(); break;
        case Tool::MoveFrames:          emit moveFramesSelected(); break;
        case Tool::Halves:              emit halvesSelected(); break;
        case Tool::SimplifySpacing:     emit simplifySpacingSelected(); break;
        case Tool::ProxySpacing:        emit proxySpacingSelected(); break;
        case Tool::MovePartials:        emit movePartialsSelected(); break;
        case Tool::GroupOrdering:       emit groupOrderingSelected(); break;
        case Tool::LocalMask:           emit localMaskSelected(); break;
        case Tool::CopyStrokes:         emit pickStrokesSelected(); break;
        case Tool::Visibility:          emit visibilitySelected(); break;
        case Tool::Debug:               emit debugSelected(); break;
        default:                        qWarning() << "No signal found for this tool (" << toolType << ")"; break;
    }
}

void ToolsManager::setTool(Tool::ToolType toolType) {
    m_previousTool = m_currentTool;
    m_currentTool = m_tools[toolType];
    if (m_previousTool) m_previousTool->toggled(false);
    if (m_currentTool) m_currentTool->toggled(true);
    signalToWindow(toolType);
    emit toolChanged(m_currentTool);
}
