#ifndef __CORRESPONDENCETOOL_H__
#define __CORRESPONDENCETOOL_H__

#include "tool.h"
#include "lassotool.h"

class CorrespondenceTool : public LassoTool {
    Q_OBJECT
public:
    CorrespondenceTool(QObject *parent, Editor *editor) : LassoTool(parent, editor) { }
    virtual ~CorrespondenceTool() = default;

    Tool::ToolType toolType() const override;
    
    void released(const EventInfo& info) Q_DECL_OVERRIDE;

private:

};

#endif // __CORRESPONDENCETOOL_H__