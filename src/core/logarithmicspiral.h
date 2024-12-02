#ifndef __LOGARITHMICSPIRAL_H__
#define __LOGARITHMICSPIRAL_H__

#include <QTransform>
#include "point.h"

class LogarithmicSpiral {
public:
    LogarithmicSpiral(){}

    Point::VectorType eval(double t) const;
    Point::VectorType evalArcLength(double s) const;

    void make(const Point::VectorType &start, const Point::VectorType &end, double rot, double scale);

    double m_rot, m_scale;
    Point::VectorType m_start, m_origin, m_end;
private:
};

#endif // __LOGARITHMICSPIRAL_H__