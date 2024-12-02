#ifndef __VISIBILITYTOOL_H__
#define __VISIBILITYTOOL_H__

#include "picktool.h"

class VisibilityTool : public PickTool {
    Q_OBJECT
public:
    VisibilityTool(QObject *parent, Editor *editor);
    virtual ~VisibilityTool() {}

    Tool::ToolType toolType() const override;

    QCursor makeCursor(float scaling=1.0f) const override;

    void toggled(bool on) override;
    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;
    void wheel(const WheelEventInfo& info) override;
    void keyReleased(QKeyEvent *event) override;
    void drawUI(QPainter &painter, VectorKeyFrame *key) override;
    void drawGL(VectorKeyFrame *key, qreal alpha) override;
    void frameChanged(int frame) override;

private:
    void setValidingCusters(bool b);

    bool m_pressed;
    bool m_validatingClusters;
    VectorKeyFrame *m_savedKeyframe;
    QList<Point *> m_lassoSelectedPoints;
};

#endif // __VISIBILITYTOOL_H__