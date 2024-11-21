#ifndef __TANGENTTOOL_H__
#define __TANGENTTOOL_H__

#include "trajectorytool.h"

class Group; 

class TangentTool : public TrajectoryTool {
    Q_OBJECT
public:
    TangentTool(QObject *parent, Editor *editor);
    virtual ~TangentTool();

    Tool::ToolType toolType() const override;

    QCursor makeCursor(float scaling=1.0f) const override;
    
    void toggled(bool on) override;
    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;
    void doublepressed(const EventInfo& info) override;
    void wheel(const WheelEventInfo& info) override;
    void drawUI(QPainter &painter, VectorKeyFrame *key) override;

private:
    bool m_p1Pressed, m_p2Pressed;
};

#endif // __TANGENTTOOL_H__