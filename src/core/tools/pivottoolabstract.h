#ifndef __PIVOTTOOLABSTRACT_H__
#define __PIVOTTOOLABSTRACT_H__

#include "tool.h"
#include "bezier2D.h"
#include "dialsandknobs.h"

class Group; 


class PivotToolAbstract : public Tool {
    Q_OBJECT
public:
    PivotToolAbstract(QObject *parent, Editor *editor);
    virtual ~PivotToolAbstract();

    QCursor makeCursor(float scaling=1.0f) const override;

protected:
    void drawPivot(QPainter &painter, int frame, float saturation = 1.);
    void drawPivot(QPainter &painter, Point::VectorType position, float angle, float saturation = 1.);
    void drawTrajectory(QPainter &painter, QVector<Bezier2D *> &beziers);
    
    void drawTrajectory(QPainter &painter);
    void drawTrajectory(QPainter &painter, QVector<VectorKeyFrame *> keys);
    
};

#endif // __PIVOTTOOLABSTRACT_H__