/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2018-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

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
#include "tools/correspondencetool.h"
#include "tools/trajectorytool.h"
#include "tools/drawtrajectorytool.h"
#include "tools/tangenttool.h"
#include "tools/fillgridtool.h"
#include "tools/directmatchingtool.h"
#include "tools/moveframestool.h"
#include "tools/halvestool.h"

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
    m_tools[Tool::Scribble]          = nullptr;
    m_tools[Tool::Traj]              = new TrajectoryTool(this, m_editor);
    m_tools[Tool::DrawTraj]          = new DrawTrajectoryTool(this, m_editor);
    m_tools[Tool::TrajTangent]       = new TangentTool(this, m_editor);
    m_tools[Tool::Lasso]             = new LassoTool(this, m_editor);
    m_tools[Tool::Correspondence]    = new CorrespondenceTool(this, m_editor);
    m_tools[Tool::FillGrid]          = new FillGridTool(this, m_editor);
    m_tools[Tool::DirectMatching]    = new DirectMatchingTool(this, m_editor);
    m_tools[Tool::MoveFrames]        = new MoveFramesTool(this, m_editor);
    m_tools[Tool::Halves]            = new HalvesTool(this, m_editor);
    m_tools[Tool::SimplifySpacing]   = nullptr;

    connect(m_tools[Tool::Pen], &Tool::updateFrame, m_editor->tabletCanvas(), &TabletCanvas::updateCurrentFrame);

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
        case Tool::Scribble:            emit scribbleSelected(); break;
        case Tool::RigidDeform:         emit deformSelected(); break;
        case Tool::Warp:                emit warpSelected(); break;
        case Tool::StrokeDeform:        emit strokeDeformSelected(); break;
        case Tool::RegistrationLasso:   emit registrationLassoSelected(); break;
        case Tool::Correspondence:      emit correspondenceSelected(); break;
        case Tool::FillGrid:            emit fillGridSelected(); break;
        case Tool::DirectMatching:      emit directMatchingSelected(); break;
        case Tool::MoveFrames:          emit moveFramesSelected(); break;
        case Tool::Halves:              emit halvesSelected(); break;
        case Tool::SimplifySpacing:     emit simplifySpacingSelected(); break;
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
