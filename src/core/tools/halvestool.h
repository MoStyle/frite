#ifndef __HALVESTOOL_H__
#define __HALVESTOOL_H__

#include "tool.h"
#include "charttool.h"

class HalvesTool : public ChartTool {
    Q_OBJECT
public:
    HalvesTool(QObject *parent, Editor *editor);
    virtual ~HalvesTool();

    virtual Tool::ToolType toolType() const override;

    virtual QCursor makeCursor(float scaling=1.0f) const override;

    virtual void tickPressed(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    virtual void tickMoved(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    virtual void tickReleased(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;
    virtual void tickDoubleClick(QGraphicsSceneMouseEvent *event, ChartTickItem *tick) override;

private:
    std::vector<float> m_offsetLeft, m_offsetRight;
};

#endif // __HALVESTOOL_H__