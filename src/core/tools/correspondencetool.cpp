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
