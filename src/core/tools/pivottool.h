#ifndef __PIVOTTOOL_H__
#define __PIVOTTOOL_H__

#include "tool.h"
#include "pivottoolabstract.h"
#include "dialsandknobs.h"

class Group; 


class PivotEditTool : public PivotToolAbstract {
    Q_OBJECT
public:
    PivotEditTool(QObject *parent, Editor *editor);
    virtual ~PivotEditTool();

    typedef enum { LAYER_TRANSLATION, PIVOT_TRANSLATION, PIVOT_TRAJECTORY, ROTATION, CONTEXT_MENU, LAYER_TRANSLATION_SELECTION } State;

    Tool::ToolType toolType() const override;


    QCursor makeCursor(float scaling=1.0f) const override;

    void toggled(bool on) override;
    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;


    void drawUI(QPainter &painter, VectorKeyFrame *key) override;


private:
    void resetPivot(int frame);
    void resetPivotToBarycenter(int frame);
    void resetPivotTrajectory(int frame);
    void resetPivotTranslation(int frame);

    bool m_pressed;
    State m_currentState;
    Point::VectorType m_currentPos;
    Point::Scalar m_angle, m_deltaAngle;
    std::vector<Point::VectorType> m_trajectoryPoints;
};

#endif // __PIVOTTOOL_H__