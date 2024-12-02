#ifndef __WARPTOOL_H__
#define __WARPTOOL_H__

#include "tool.h"

class Group; 

class WarpTool : public Tool {
    Q_OBJECT
public:
    WarpTool(QObject *parent, Editor *editor);
    virtual ~WarpTool();

    Tool::ToolType toolType() const override;

    QCursor makeCursor(float scaling=1.0f) const override;
    
    void toggled(bool on) override;
    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;
    void doublepressed(const EventInfo& info) override;
    void wheel(const WheelEventInfo& info) override;
    void drawUI(QPainter &painter, VectorKeyFrame *key) override;
    void drawGL(VectorKeyFrame *key, qreal alpha) override;

protected:
    QVector2D m_nudge;
    bool m_pressed;
private:
    Point::VectorType m_pivot;
    Point::Affine m_inverseRigidGlobal;
    bool m_registerToNextKeyframe;
};

#endif // __WARPTOOL_H__