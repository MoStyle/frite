#ifndef __MOVEPARTIALSTOOL_H__
#define __MOVEPARTIALSTOOL_H__

#include "tool.h"
#include "charttool.h"
#include "partial.h"

class Group; 
class Trajectory;

class MovePartialsTool : public ChartTool {
    Q_OBJECT
public:
    MovePartialsTool(QObject *parent, Editor *editor);
    virtual ~MovePartialsTool();

    Tool::ToolType toolType() const override;

    QCursor makeCursor(float scaling=1.0f) const override;
    
    void tickPressed(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    void tickMoved(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    void tickReleased(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    void tickDoubleClick(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    void tickPressed(const EventInfo &info);
    void tickMoved(const EventInfo &info);
    void tickReleased(const EventInfo &info);
    void drawUI(QPainter &painter, VectorKeyFrame *key) override;

public slots:
    void restoreAndClearState() const;
    void removeIdenticalOrderPartials(VectorKeyFrame *keyframe) const;
    void removeIdenticalDrawingPartials(VectorKeyFrame *keyframe) const;

private:
    int m_prevSelectedGroup;
    bool m_partialTickPressed;
    unsigned int m_partialTickPressedId;
    Trajectory *m_partialTrajectoryPressed;
    Partials<OrderPartial> m_savedStateOrder;
    Partials<DrawingPartial> m_savedStateDrawing;
};

#endif // __MOVEPARTIALSTOOL_H__