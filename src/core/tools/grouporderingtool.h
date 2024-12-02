#ifndef __GROUPORDERINGTOOL_H__
#define __GROUPORDERINGTOOL_H__

#include "tool.h"
#include "charttool.h"
#include "partial.h"

class Group; 
class Trajectory;

class GroupOrderingTool : public ChartTool {
    Q_OBJECT
public:
    GroupOrderingTool(QObject *parent, Editor *editor);
    virtual ~GroupOrderingTool();

    Tool::ToolType toolType() const override;

    QCursor makeCursor(float scaling=1.0f) const override;
    
    void toggled(bool on) override;
    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;
    void doublepressed(const EventInfo& info) override;
    void tickPressed(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    void tickMoved(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    void tickReleased(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    void tickPressed(const EventInfo &info);
    void tickMoved(const EventInfo &info);
    void tickReleased(const EventInfo &info);
    void tickDoubleClick(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    void drawUI(QPainter &painter, VectorKeyFrame *key) override;
    void contextMenu(QMenu &contextMenu) override;

public slots:
    void restoreAndClearState() const;
    void frameChanged(int frame) override;

private:
    void removeIdenticalPartials(VectorKeyFrame *keyframe) const;

    int m_prevSelectedGroup;
    bool m_partialTickPressed;
    unsigned int m_partialTickPressedId;
    Trajectory *m_partialTrajectoryPressed;
    QFontMetrics m_fontMetrics;
    Partials<OrderPartial> m_savedState;
    VectorKeyFrame *m_prevKeyframe;
};

#endif // __GROUPORDERINGTOOL_H__