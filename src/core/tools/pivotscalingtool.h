#ifndef __PIVOTSCALINGTOOL_H__
#define __PIVOTSCALINGTOOL_H__

#include "tool.h"
#include "pivottoolabstract.h"
#include "dialsandknobs.h"

class Group; 


class PivotScalingTool : public PivotToolAbstract {
    Q_OBJECT
public:
    PivotScalingTool(QObject *parent, Editor *editor);
    virtual ~PivotScalingTool();

    typedef enum { SCALING, CONTEXT_MENU } State;

    Tool::ToolType toolType() const override;

    QCursor makeCursor(float scaling=1.0f) const override;
    
    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;
    void drawUI(QPainter &painter, VectorKeyFrame *key) override;
    

private:
    void applyMirroring(int frame, bool xAxis, bool yAxis);

    State m_currentState;
    Point::VectorType m_firstPos;
    Point::VectorType m_point;
    bool m_pressed;

};

#endif // __PIVOTSCALINGTOOL_H__