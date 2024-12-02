#ifndef __PENTOOL_H__
#define __PENTOOL_H__

#include "tool.h"
#include "dialsandknobs.h"
#include "stroke.h"

class QGraphicsItem;
class Stroke;

class PenTool : public Tool {
    Q_OBJECT
public:
    PenTool(QObject *parent, Editor *editor);
    virtual ~PenTool();

    virtual Tool::ToolType toolType() const override;

    virtual QCursor makeCursor(float scaling=1.0f) const override;

    virtual void pressed(const EventInfo& info) Q_DECL_OVERRIDE;
    virtual void moved(const EventInfo& info) Q_DECL_OVERRIDE;
    virtual void released(const EventInfo& info) Q_DECL_OVERRIDE;
    virtual void wheel(const WheelEventInfo& info) Q_DECL_OVERRIDE;

    QPen &pen() { return m_pen; }

    Stroke *currentStroke() const { return m_currentStroke.get(); }

protected:
    void addPoint(const EventInfo &info);

    QBrush m_brush;
    QPen m_pen;
    StrokePtr m_currentStroke;
    double m_startTime, m_curTime;
    bool m_pressed;
};

#endif // __PENTOOL_H__