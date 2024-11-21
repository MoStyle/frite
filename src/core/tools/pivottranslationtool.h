#ifndef __PIVOTTRANSLATIONTOOL_H__
#define __PIVOTTRANSLATIONTOOL_H__

#include "tool.h"
#include "dialsandknobs.h"

class Group; 


class PivotTranslationTool : public Tool {
    Q_OBJECT
public:
    PivotTranslationTool(QObject *parent, Editor *editor);
    virtual ~PivotTranslationTool();

    typedef enum { TRAJECTORY, MOVE_FIRST_POINT, CONTEXT_MENU } State;

    Tool::ToolType toolType() const override;

    QCursor makeCursor(float scaling=1.0f) const override;
    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;

    void drawUI(QPainter &painter, VectorKeyFrame *key) override;


private:
    void drawTrajectory(QPainter &painter, VectorKeyFrame * keyFrame);

    State m_currentState;
    Point::VectorType m_mouseTranslation;
    Point::Scalar m_angle, m_deltaAngle;
    bool m_pressed;
    Point::VectorType m_currentPos;
    std::vector<Point::VectorType> m_trajectoryPoints;

};

#endif // __PIVOTTRANSLATIONTOOL_H__