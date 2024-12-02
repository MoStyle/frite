#ifndef __FILLGRIDTOOL_H__
#define __FILLGRIDTOOL_H__

#include "tool.h"

class VectorKeyFrame;
class Group;

class FillGridTool : public Tool {
    Q_OBJECT
public:
    FillGridTool(QObject *parent, Editor *editor);
    virtual ~FillGridTool();

    Tool::ToolType toolType() const override;

    QCursor makeCursor(float scaling=1.0f) const override;
    
    void toggled(bool on) override;
    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;
    void doublepressed(const EventInfo& info) override;
    void wheel(const WheelEventInfo& info) override;
    void drawUI(QPainter &painter, VectorKeyFrame *key) override;
    void contextMenu(QMenu &contextMenu) override;

private:
    bool addQuad(VectorKeyFrame *keyframe, Group *group, const Point::VectorType &pos);
    bool removeQuad(VectorKeyFrame *keyframe, Group *group, const Point::VectorType &pos);
};

#endif // __FILLGRIDTOOL_H__