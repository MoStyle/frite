#ifndef __ERASERTOOL_H__
#define __ERASERTOOL_H__

#include "tool.h"

class EraserTool : public Tool {
    Q_OBJECT
public:
    EraserTool(QObject *parent, Editor *editor);

    Tool::ToolType toolType() const override;

    QCursor makeCursor(float scaling=1.0f) const override;

    void toggled(bool on) override;
    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;
    void wheel(const WheelEventInfo& info) override;

    void drawGL(VectorKeyFrame *key, qreal alpha) override;

private:
    void erase(const EventInfo& info);
    void eraseSegments(const EventInfo& info);

    bool m_pressed;
    int m_prevFrame, m_frame;
    VectorKeyFrame *m_keyframe;
    QHash<unsigned int, double> m_savedVisibility;
};

#endif // __ERASERTOOL_H__