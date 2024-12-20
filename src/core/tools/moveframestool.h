#ifndef __MOVEFRAMESTOOL_H__
#define __MOVEFRAMESTOOL_H__

#include "tool.h"
#include "charttool.h"

class MoveFramesTool : public ChartTool {
    Q_OBJECT
public:
    MoveFramesTool(QObject *parent, Editor *editor);
    virtual ~MoveFramesTool();

    virtual Tool::ToolType toolType() const override;

    virtual QCursor makeCursor(float scaling=1.0f) const override;

    virtual void tickPressed(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    virtual void tickMoved(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    virtual void tickReleased(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    virtual void tickDoubleClick(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;

private:
    std::vector<float> m_offsetLeft, m_offsetRight;
};

#endif // __MOVEFRAMESTOOL_H__