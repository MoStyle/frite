/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "correspondencetool.h"

#include "editor.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "canvascommands.h"
#include "strokeinterval.h"
#include "group.h"

Tool::ToolType CorrespondenceTool::toolType() const {
    return Tool::Correspondence;
}

void CorrespondenceTool::released(const EventInfo& info) {

}
