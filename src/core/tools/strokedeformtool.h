#ifndef __STROKEDEFORMTOOL_H__
#define __STROKEDEFORMTOOL_H__

#include "tool.h"
#include "stroke.h"

class StrokeDeformTool : public Tool {
    Q_OBJECT
public:
    StrokeDeformTool(QObject *parent, Editor *editor);
    virtual ~StrokeDeformTool();

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
    QBrush m_brush;
    QPen m_pen;
    StrokePtr m_currentStroke;
    double m_startTime, m_curTime;
    bool m_pressed;
};

#endif // __STROKEDEFORMTOOL_H__