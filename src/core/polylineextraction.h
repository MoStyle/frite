#ifndef POLYLINEEXTRACTION

#include <vector>
#include <QTransform>

#include "point.h"

class PolylineExtraction {
public:
    static bool extract(const std::vector<Point *>& points, const std::vector<Point::VectorType>& aggregate,
            std::vector<Point::VectorType>& oPolyline);
    static void separate(const std::vector<Point *>& points, const Point::VectorType& point, 
            const std::vector<Point::VectorType>& aggregate, std::vector<Point::VectorType>& A, std::vector<Point::VectorType>& B);
};

#endif // !POLYLINEEXTRACTION