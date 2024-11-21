#ifndef __PICKTOOL_H__
#define __PICKTOOL_H__

#include "tool.h"

class Group;
class LassoDrawer;

class PickTool : public Tool {
    Q_OBJECT
public:
    PickTool(QObject *parent, Editor *editor);
    virtual ~PickTool();

    virtual Tool::ToolType toolType() const override;

    virtual QCursor makeCursor(float scaling=1.0f) const override;

    void toggled(bool on) override;
    virtual void pressed(const EventInfo& info) override;
    virtual void moved(const EventInfo& info) override;
    virtual void released(const EventInfo& info) override;
    void keyPressed(QKeyEvent *event) override;
    void keyReleased(QKeyEvent *event) override;
    virtual void drawUI(QPainter &painter, VectorKeyFrame *key) override;

    virtual const QPolygonF &selectionPolygon() const { return m_lasso; }

    bool selectingTarget() const { return m_selectInTarget; };


signals:
    void newSelectedGroup(Group *group);

public slots:
    void setDrawEndKeyframe(int index);

protected:
    bool m_selectInTarget;
    QPolygonF m_lasso;
    bool m_pressed;
};

#endif // __PICKTOOL_H__