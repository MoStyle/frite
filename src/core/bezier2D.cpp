#include "bezier2D.h"
#include "utils/utils.h"
#include "utils/geom.h"
#include "dialsandknobs.h"

#include <Eigen/Dense>
#include <iostream>

Bezier2D::Bezier2D() : p0(Point::VectorType::Zero()), p1(Point::VectorType(0.5, 0.5)), p2(Point::VectorType(0.5, 0.5)), p3(Point::VectorType(1.0, 1.0)) {
    updateArclengthLUT();
}

Bezier2D::Bezier2D(Point::VectorType _p0, Point::VectorType _p1, Point::VectorType _p2, Point::VectorType _p3) 
    : p0(_p0), p1(_p1), p2(_p2), p3(_p3) {
    updateArclengthLUT();
}

Bezier2D::Bezier2D(const Bezier2D &other) 
    : p0(other.p0), p1(other.p1), p2(other.p2), p3(other.p3) {
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < LUT_PRECISION; ++j) {
            alengthLUT[i][j] = other.alengthLUT[i][j];
        }
    }
}

Point::VectorType Bezier2D::eval(Point::Scalar t) const {
    Point::Scalar tx = 1.0 - t;
    return p0 * (tx * tx * tx) +
           p1 * (3.0 * tx * tx * t) +
           p2 * (3.0 * tx * t * t) +
           p3 * (t * t * t);
}

Point::VectorType Bezier2D::evalDer(Point::Scalar t) const {
    Point::Scalar tx = 1.0 - t;
    return (p1 - p0) * (3.0 * tx * tx) +
           (p2 - p1) * (6.0 * tx * t) + 
           (p3 - p2) * (3.0 * t * t);
}

Point::VectorType Bezier2D::evalArcLength(Point::Scalar s) const {
    return eval(param(s));
}

Point::VectorType Bezier2D::evalDerArcLength(Point::Scalar s) const {
    return evalDer(param(s));
}

Point::Scalar Bezier2D::evalYFromX(Point::Scalar x) {
    Point::Scalar t = tFromX(x);
    Eigen::Vector4d coeffs = Geom::bezierCoeffs(p0.y(), p1.y(), p2.y(), p3.y());
    return (t * t * t * coeffs[0]) + (t * t * coeffs[1]) + (t * coeffs[2]) + coeffs[3];
}

// get s from t
Point::Scalar Bezier2D::arcLength(Point::Scalar t) const {
    return 0.0;   
}

Point::Scalar Bezier2D::speed(Point::Scalar t) const {
    return evalDer(t).norm();
}

void Bezier2D::fit(const std::vector<Point::VectorType> &data, bool constrained) {
    std::vector<Point::Scalar> u;
    chordLengthParameterize(data, u);

    constrained ? fitBezierConstrained(data, u) : fitBezier(data, u);
    Point::Scalar err = maxError(data, u);

    for (int i = 0; i < 4; ++i) {
        reparameterize(data, u);
        constrained ? fitBezierConstrained(data, u) : fitBezier(data, u);
        maxError(data, u);
    }
}

void Bezier2D::fitWithParam(const std::vector<Point::VectorType> &data, const std::vector<Point::Scalar> &u) {
    fitBezierConstrained(data, u);
    maxError(data, u);
}

// transform the current control points so that the endpoints best align with the given start and end points
void Bezier2D::fitExtremities(Point::VectorType start, Point::VectorType end) {
    // original basis
    Point::VectorType t1 = (p3 - p0).normalized();
    Point::VectorType t2(-t1.y(), t1.x());
    
    // diff coordinates
    Point::VectorType p1Diff, p2Diff;
    p1Diff = Point::VectorType((p1 - p0).dot(t1), (p1 - p0).dot(t2));
    p2Diff = Point::VectorType((p2 - p0).dot(t1), (p2 - p0).dot(t2));

    // new basis
    t1 = (end - start).normalized();
    t2 = Point::VectorType(-t1.y(), t1.x());

    // scaling factor
    Point::Scalar l1 = (p3 - p0).norm();
    Point::Scalar l2 = (end - start).norm();
    Point::Scalar l;
    if (l2 == 0.0 || l1 == 0.0) l = 1.0;
    else l = l2 / l1;
    p1Diff *= l;
    p2Diff *= l;

    // new control points
    p0 = start;
    p1 = p0 + t1 * p1Diff.x() + t2 * p1Diff.y();
    p2 = p0 + t1 * p2Diff.x() + t2 * p2Diff.y();
    p3 = end;
}

void Bezier2D::split(Point::Scalar t, Bezier2D &left, Bezier2D &right) const {
    // de casteljau
    Point::VectorType p01 = (p1 - p0) * t + p0;
    Point::VectorType p12 = (p2 - p1) * t + p1;
    Point::VectorType p23 = (p3 - p2) * t + p2;
    Point::VectorType p012 = (p12 - p01) * t + p01;
    Point::VectorType p123 = (p23 - p12) * t + p12;
    Point::VectorType p0123 = (p123 - p012) * t + p012;

    left.setP0(p0);
    left.setP1(p01);
    left.setP2(p012);
    left.setP3(p0123);
    right.setP0(p0123);
    right.setP1(p123);
    right.setP2(p23);
    right.setP3(p3);
}

void Bezier2D::updateArclengthLUT() {
    double step = 1.0 / (LUT_PRECISION - 1);
    double t = 0.0;
    double s = 0.0;
    Point::VectorType cur, prev;

    alengthLUT[0][0] = 0.0;
    alengthLUT[1][0] = 0.0;
    alengthLUT[0][LUT_PRECISION - 1] = 1.0;
    alengthLUT[1][LUT_PRECISION - 1] = 1.0;

    prev = p0;
    
    for (int i = 1; i < LUT_PRECISION - 1; i++) {
        t += step;
        cur = eval(t);
        s += (cur - prev).norm();
        prev = cur;
        alengthLUT[0][i] = s;
        alengthLUT[1][i] = t;
    }
    cur = eval(1.0);
    s += (cur - prev).norm();
    len = s;

    // normalize s
    for (int i = 1; i < LUT_PRECISION - 1; i++) {
        alengthLUT[0][i] /= s;
    }
}

void Bezier2D::translate(Point::VectorType translation){
    p0 += translation;
    p1 += translation;
    p2 += translation;
    p3 += translation;
}

bool Bezier2D::load(QDomElement &element){
    if (element.tagName() != "bezier2D") return false;

    QString string = element.text();
    QTextStream stream(&string);
    Point::Scalar p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y;
    stream >> p0x >> p0y >> p1x >> p1y >> p2x >> p2y >> p3x >> p3y;

    setP0(Point::VectorType(p0x, p0y));
    setP1(Point::VectorType(p1x, p1y));
    setP2(Point::VectorType(p2x, p2y));
    setP3(Point::VectorType(p3x, p3y));
    return true;
}

bool Bezier2D::save(QDomDocument &doc, QDomElement &root) const{
    QDomElement bezierElt = doc.createElement("bezier2D");
    QString string;
    QTextStream points(&string);
    points << p0.x() << " " << p0.y() << " ";
    points << p1.x() << " " << p1.y() << " ";
    points << p2.x() << " " << p2.y() << " ";
    points << p3.x() << " " << p3.y() << " ";

    QDomText txt = doc.createTextNode(string);
    bezierElt.appendChild(txt);
    root.appendChild(bezierElt);
    return true;
}

Point::Scalar Bezier2D::tFromX(Point::Scalar x) {
    // TODO test x boundaries
    Eigen::Vector4d coeffs = Geom::bezierCoeffs(p0.x(), p1.x(), p2.x(), p3.x());

    if (std::abs(coeffs[0]) < 1e-8) {
        if (std::abs(coeffs[1]) < 1e-8) { // linear
            return (x - coeffs[3]) / coeffs[2];
        }
        return Utils::quadraticRoot(coeffs[1], coeffs[2], coeffs[3] - x);
    }

    return Utils::cubicRoot(coeffs[1] / coeffs[0], coeffs[2] / coeffs[0], (coeffs[3] - x) / coeffs[0]);
}

void Bezier2D::debug() const {
    std::cout << "P0: " << p0.transpose() << std::endl;
    std::cout << "P1: " << p1.transpose() << std::endl;
    std::cout << "P2: " << p2.transpose() << std::endl;
    std::cout << "P3: " << p3.transpose() << std::endl;
    std::cout << "length: " << length() << std::endl;
    std::cout << "--------------" << std::endl;
}

void Bezier2D::fitBezier(const std::vector<Point::VectorType> &data, const std::vector<Point::Scalar> &u) {
    Eigen::Matrix4d M;  // bezier coeffs
    Eigen::MatrixXd T;  // param
    Eigen::MatrixXd D;  // data points
    Eigen::MatrixXd P;  // control points (unknowns)

    M << -1.0, 3.0, -3.0, 1.0,
          3.0, -6.0, 3.0, 0.0,
         -3.0, 3.0, 0.0, 0.0,
          1.0, 0.0, 0.0, 0.0;

    T = Eigen::MatrixXd(data.size(), 4);
    D = Eigen::MatrixXd(data.size(), 2);
    for (int i = 0; i < data.size(); ++i) {
        D.row(i) = data[i];
        T.coeffRef(i, 3) = 1.0;
        for (int j = 2; j >= 0; --j) {
            T.coeffRef(i, j) = u[i] * T.coeffRef(i, j + 1);
        }
    }

    P = Eigen::MatrixXd(4, 2);
    P = (T * M).colPivHouseholderQr().solve(D);

    p0 = P.row(0);
    p1 = P.row(1);
    p2 = P.row(2);
    p3 = P.row(3);
}

/**
 * TODO refactor constraint row/col
 * Cubic fit with fixed boudary constraints
 */
void Bezier2D::fitBezierConstrained(const std::vector<Point::VectorType> &data, const std::vector<Point::Scalar> &u) {
    Eigen::Matrix4d M;  // bezier coeffs
    Eigen::MatrixXd T;  // param
    Eigen::MatrixXd D;  // data points
    Eigen::MatrixXd P;  // control points (unknowns)

    M << -1.0, 3.0, -3.0, 1.0,
          3.0, -6.0, 3.0, 0.0,
         -3.0, 3.0, 0.0, 0.0,
          1.0, 0.0, 0.0, 0.0;

    T = Eigen::MatrixXd(data.size(), 4);
    D = Eigen::MatrixXd(data.size(), 2);
    for (int i = 0; i < data.size(); ++i) {
        D.row(i) = data[i];
        T.coeffRef(i, 3) = 1.0;
        for (int j = 2; j >= 0; --j) {
            T.coeffRef(i, j) = u[i] * T.coeffRef(i, j + 1);
        }
    }

    Eigen::MatrixXd A = T * M;
    Eigen::MatrixXd ATA = A.transpose() * A;
    Eigen::MatrixXd DD = A.transpose() * D;
    Eigen::MatrixXd B(ATA.rows() + 2, ATA.cols() + 2);
    Eigen::MatrixXd E(DD.rows() + 2, DD.cols());
    Eigen::Vector<double, 6> V1(1.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    Eigen::Vector<double, 6> V2(0.0, 0.0, 0.0, 1.0, 0.0, 0.0);
    B.topLeftCorner<>(ATA.rows(), ATA.cols()) = ATA;
    B.row(ATA.rows()) = V1;
    B.row(ATA.rows()+1) = V2;
    B.col(ATA.cols()) = V1.transpose();
    B.col(ATA.cols()+1) = V2.transpose();

    E.topLeftCorner(DD.rows(), DD.cols()) = DD;
    E.row(DD.rows()) = data[0];
    E.row(DD.rows()+1) = data.back();

    P = Eigen::MatrixXd(6, 2);
    P = (B).ldlt().solve(E);

    p0 = P.row(0);
    p1 = P.row(1);
    p2 = P.row(2);
    p3 = P.row(3);
}


void Bezier2D::chordLengthParameterize(const std::vector<Point::VectorType> &data, std::vector<Point::Scalar> &u) {
    if (data.empty()) {
        std::cout << "chordLengthParameterize: insufficient data points" << std::endl;
    }

    u.resize(data.size());
    u[0] = Point::Scalar(0.0);

    for (int i = 1; i < data.size(); ++i) {
        u[i] = (data[i] - data[i - 1]).norm() + u[i - 1];
    }

    for (int i = 1; i < data.size(); ++i) {
        u[i] /= u.back();
    }
}

void Bezier2D::reparameterize(const std::vector<Point::VectorType> &data, std::vector<Point::Scalar> &u) {
    for (int i = 0; i < data.size(); ++i) {
        u[i] = newtonRaphsonRootFind(data[i], u[i]);
    }
}

Point::Scalar Bezier2D::newtonRaphsonRootFind(const Point::VectorType& data, Point::Scalar param) {
    Point::Scalar num, den;

    Point::VectorType p = eval(param);
    Point::VectorType _pp[3];
    Point::VectorType _ppp[2];

    _pp[0] = (p1 - p0) * 3.0;
    _pp[1] = (p2 - p1) * 3.0;
    _pp[2] = (p3 - p2) * 3.0;

    _ppp[0] = (_pp[1] - _pp[0]) * 2.0;
    _ppp[1] = (_pp[2] - _pp[1]) * 2.0;

    Point::Scalar ux = 1.0 - param;
    Point::VectorType pp = ux * ux * _pp[0] + 2.0 * ux * param * _pp[1] + param * param * _pp[2];
    Point::VectorType ppp = _ppp[0] * ux + _ppp[1] * param;

    num = (p.x() - data.x()) * pp.x() + (p.y() - data.y()) * pp.y();
    den = pp.x() * pp.x() + pp.y() * pp.y() + (p.x() - data.x()) * ppp.x() + (p.y() - data.y()) * ppp.y();

    if (den < 1e-6) return param;
    return param - (num / den);
}

/**
 * Returns the maximum error (L2 norm) between the data and the fitted cubic 
 */
Point::Scalar Bezier2D::maxError(const std::vector<Point::VectorType> &data, const std::vector<Point::Scalar> &u) {
    Point::Scalar maxError = std::numeric_limits<Point::Scalar>::min();
    Point::Scalar error;
    // std::cout << "*********"<< std::endl;
    for (int i = 0; i < data.size(); ++i) {
        error = (data[i] - eval(u[i])).norm();
        // std::cout << "data[i] = " << data[i].transpose() << "     vs      " <<  eval(u[i]).transpose()  << std::endl;
        // std::cout << "error at u=" << u[i] << " : " << error<< std::endl;
        if (error > maxError) maxError = error;
    }
    // std::cout << "Max error = " << maxError << std::endl;
    return maxError;
}

Point::Scalar Bezier2D::totalLength() const {
    Point::Scalar s = 0.0;
    Point::VectorType cur, prev;

    prev = p0;
    
    for (int i = 1; i < LUT_PRECISION - 1; i++) {
        cur = eval(i);
        s += (cur - prev).norm();
        prev = cur;
    }
    cur = eval(1.0);
    s += (cur - prev).norm();

    return s;
}

/******************************************************************************************************************************/

CompositeBezier2D::CompositeBezier2D()
    : m_continuity(1) {
    addControlPoint(0, Point::VectorType(0, 0));
}

CompositeBezier2D::~CompositeBezier2D(){
    for (Bezier2D * bezier : m_beziers)
        delete bezier;
}

void CompositeBezier2D::addBezierCurve(uint idx){
    uint nbBeziers = m_beziers.size();
    if (nbBeziers == 0){
        Point::VectorType pointZero(0, 0);
        m_beziers.push_back(new Bezier2D(pointZero, pointZero, pointZero, pointZero));
        m_trajectoryExists.push_back(false);
        m_breakContinuity.push_back(false);
        return;
    }

    idx = idx < nbBeziers ? idx : nbBeziers;
    Point::VectorType pointZero(0, 0);
    Bezier2D * newBezier = new Bezier2D(pointZero, pointZero, pointZero, pointZero); 
    Bezier2D left, right;

    if (idx == 0){
        newBezier->setP3(m_beziers[0]->getP0());
        m_beziers.emplace(m_beziers.begin(), newBezier);
    }
    else if (idx == nbBeziers){
        newBezier->setP3(m_beziers[nbBeziers - 1]->getP3());
        m_beziers.emplace(m_beziers.end(), newBezier);
    }
    else{
        newBezier->setP3(m_beziers[idx]->getP0());
        m_beziers.emplace(m_beziers.begin() + idx, newBezier);
    }

    m_trajectoryExists.emplace(m_trajectoryExists.begin() + idx, false);
    m_breakContinuity.emplace(m_breakContinuity.begin() + idx, false);
}

void CompositeBezier2D::recomputeIntermediatePoint(uint idx){
    if (idx >= m_beziers.size() || m_trajectoryExists[idx])
        return;
    const qreal alpha = 1. / 3.;
    m_beziers[idx]->setP1(m_beziers[idx]->getP0() * (1 - alpha) + m_beziers[idx]->getP3() * alpha);
    m_beziers[idx]->setP2(m_beziers[idx]->getP3() * (1 - alpha) + m_beziers[idx]->getP0() * alpha);
}

void CompositeBezier2D::updateBezier(uint idx){
    int nbBeziers = m_beziers.size();
    if (idx >= nbBeziers)
        return;

    recomputeIntermediatePoint(idx);

    if (idx < nbBeziers - 1){
        m_beziers[idx + 1]->setP0(m_beziers[idx]->getP3());
        if (!m_trajectoryExists[idx + 1])
            recomputeIntermediatePoint(idx + 1);
    }
    if (idx > 0){
        m_beziers[idx - 1]->setP3(m_beziers[idx]->getP0());
        if (!m_trajectoryExists[idx - 1])
            recomputeIntermediatePoint(idx - 1);
    }
    applyContinuity();
}

void CompositeBezier2D::setP0(uint idx, Point::VectorType point){
    if (idx >= m_beziers.size())
        return;
    m_beziers[idx]->setP0(point);
}

void CompositeBezier2D::addControlPoint(Point::Scalar t, Point::VectorType point){
    static const float epsilon = 1e-6;
    if (t > 1.)
        t = 1.;
    if (m_times.empty() || t > m_times.back()){
        int idx = m_times.size();
        m_times.emplace(m_times.end(), t);
        addBezierCurve(idx);
        setP0(idx, point);
        m_beziers[idx]->setP3(point);
        updateBezier(idx);
        return;
    }
    for (int i = 0; i < m_times.size(); i++){
        if (m_times[i] >= t - epsilon && m_times[i] <= t + epsilon){
            setP0(i, point);
            if (m_times.size() - 1 == i)
                m_beziers[i]->setP3(point);
            updateBezier(i);
            return;
        }
        else if (t < m_times[i]){
            m_times.emplace(m_times.begin() + i, t);
            addBezierCurve(i);
            setP0(i, point);
            updateBezier(i);
            return;
        }
    }
}

void CompositeBezier2D::moveControlPoint(Point::Scalar tSrc, Point::Scalar tDst){
    static const float epsilon = 1e-6;
    for (int i = 0; i < m_times.size(); ++i){
        if (m_times[i] >= tSrc - epsilon && m_times[i] <= tSrc + epsilon){
            m_times[i] = tDst;
            return;
        }
    }
}

void CompositeBezier2D::deleteControlPoint(Point::Scalar t){
    static const float epsilon = 1e-6;
    for (int i = 0; i < m_times.size(); ++i){
        if (m_times[i] >= t - epsilon && m_times[i] <= t + epsilon){
            if (i == m_times.size() - 1){
                m_beziers[i - 1]->setP3(m_beziers[i]->getP3());
            }
            else if (m_times.size() > 1){
                m_beziers[i + 1]->setP0(m_beziers[i - 1]->getP3());
                updateBezier(i + 1);
            }
            m_times.erase(m_times.begin() + i);
            m_beziers.erase(m_beziers.begin() + i);
            m_trajectoryExists.erase(m_trajectoryExists.begin() + i);
            m_breakContinuity.erase(m_breakContinuity.begin() + i);
            applyContinuity();
            return;
        }
    }
}

void CompositeBezier2D::translateControlPoint(Point::Scalar t, Point::VectorType translation){
    static const float epsilon = 1e-6;
    for (int i = 0; i < m_times.size(); ++i){
        if (m_times[i] >= t - epsilon && m_times[i] <= t + epsilon){
            Bezier2D * bezier = m_beziers[i];
            bezier->setP0(bezier->getP0() + translation);
            bezier->setP1(bezier->getP1() + translation);
            bezier->setP2(bezier->getP2() + translation);
            if (i == m_times.size() - 1)
                bezier->setP3(bezier->getP3() + translation);
            addControlPoint(t, bezier->getP0());
            replaceBezierCurve(bezier, t, true);
            updateBezier(i);
            return;
        }
    }
}

Point::VectorType CompositeBezier2D::getNextControlPoint(Point::Scalar t){
    static const float epsilon = 1e-6;
    for (int i = 0; i < m_times.size(); ++i){
        if (i == m_times.size() - 1)
            return m_beziers[i]->getP0();
        if (m_times[i] >= t - epsilon && m_times[i] <= t + epsilon)
            return m_beziers[i + 1]->getP0();
        if (t > m_times[i] && t < m_times[i + 1])
            return m_beziers[i + 1]->getP0();
    }
    return Point::VectorType(NAN, NAN);
}

Point::VectorType CompositeBezier2D::evalArcLength(Point::Scalar t){
    // Debug ...
    // for (Bezier2D bezier : m_beziers){
        // qDebug() << "Bezier : P0 = " << bezier.getP0().x() << ", " << bezier.getP0().y();
        // qDebug() << "P1 = " << bezier.getP1().x() << ", " << bezier.getP1().y();
        // qDebug() << "P2 = " << bezier.getP2().x() << ", " << bezier.getP2().y();
        // qDebug() << "P3 = " << bezier.getP3().x() << ", " << bezier.getP3().y();
    // }
    // ...

    static const float epsilon = 1e-6;
    if (t >= 1.){
        if (m_times.back() > 1. - epsilon)
            return m_beziers.back()->evalArcLength(0.0);
        else
            return m_beziers.back()->evalArcLength(1.0);
    }
    if (t <= 0.)
        return m_beziers.front()->evalArcLength(0.0);

    float prevTime = 0.f;
    float nextTime = 1.f;
    for (int i = 0; i < m_times.size(); ++i){
        if (i == m_times.size() - 1){
            prevTime = m_times[i];
        }
        else if (t >= m_times[i] && t <= m_times[i + 1]){
            prevTime = m_times[i];
            nextTime = m_times[i + 1];
        }
        else continue;

        float s = (t - prevTime) / (nextTime - prevTime);
        if (std::isinf(s)) s = 0.f;
        Bezier2D * bezier = m_beziers[i];
        if (bezier->getP0() == bezier->getP3())
            return bezier->getP0();

        return m_beziers[i]->evalArcLength(s);
    }

    return m_beziers.back()->evalArcLength(1.0);
}

Bezier2D * CompositeBezier2D::getBezier(float t) {
    static const float epsilon = 1e-6;
    for (int i = 0; i < m_times.size(); ++i){
        if (m_times[i] >= t - epsilon && m_times[i] <= t + epsilon){           
            return m_beziers[i];
        }
    }
    return nullptr;
}

void CompositeBezier2D::keepTrajectory(float t, bool keep){
    static const float epsilon = 1e-6;
    for (int i = 0; i < m_times.size(); ++i){
        if (m_times[i] >= t - epsilon && m_times[i] <= t + epsilon){
            m_trajectoryExists[i] = keep;
        }
    }
}

bool CompositeBezier2D::isTrajectoryKeeped(float t){
    static const float epsilon = 1e-6;
    for (int i = 0; i < m_times.size(); ++i){
        if (m_times[i] >= t - epsilon && m_times[i] <= t + epsilon){
            return m_trajectoryExists[i];
        }
    }
    return false;
}

bool CompositeBezier2D::hasControlPoint(float t){
    static const float epsilon = 1e-6;
    for (int i = 0; i < m_times.size(); ++i)
        if (m_times[i] >= t - epsilon && m_times[i] <= t + epsilon) return true;
    return false;
}

void CompositeBezier2D::breakContinuity(float t, bool value){
    static const float epsilon = 1e-6;
    for (int i = 0; i < m_times.size(); ++i){
        if (m_times[i] >= t - epsilon && m_times[i] <= t + epsilon)
            m_breakContinuity[i] = value;
    }
}

bool CompositeBezier2D::isContinuityBroken(float t){
    static const float epsilon = 1e-6;
    for (int i = 0; i < m_times.size(); ++i){
        if (m_times[i] >= t - epsilon && m_times[i] <= t + epsilon) return m_breakContinuity[i];
    }
    return false;
}


bool CompositeBezier2D::load(QDomElement &element){
    if (element.tagName() != "compositebezier") return false;

    m_continuity = element.attribute("continuity").toFloat();
    m_beziers.clear();
    m_times.clear();
    m_trajectoryExists.clear();
    m_breakContinuity.clear();


    QDomElement bezierElt = element.firstChildElement("bezier");
    while(!bezierElt.isNull()){

        Bezier2D * bezier = new Bezier2D();
        QDomElement bezier2DElt = bezierElt.firstChildElement("bezier2D");
        if (!bezier->load(bezier2DElt))
            return false;
        m_beziers.push_back(bezier);
        m_times.push_back(bezierElt.attribute("time").toFloat());
        m_trajectoryExists.push_back(bezierElt.attribute("trajexists") != "0");
        m_breakContinuity.push_back(bezierElt.attribute("breakcontinuity") != "0");
        bezierElt = bezierElt.nextSiblingElement("bezier");
    }

    return true;
}

bool CompositeBezier2D::save(QDomDocument &doc, QDomElement &root) const{
    QDomElement compositeElt = doc.createElement("compositebezier");
    for (int i = 0; i < m_beziers.size(); ++i){
        QDomElement bezierElt = doc.createElement("bezier");
        bezierElt.setAttribute("time", m_times[i]);
        bezierElt.setAttribute("trajexists", m_trajectoryExists[i] ? "1" : "0");
        bezierElt.setAttribute("breakcontinuity", m_breakContinuity[i] ? "1" : "0");
        m_beziers[i]->save(doc, bezierElt);
        compositeElt.appendChild(bezierElt);
    }
    compositeElt.setAttribute("continuity", m_continuity);

    root.appendChild(compositeElt);
    return true;
}

void CompositeBezier2D::changeContinuity(int C){
    if (C < 0 || C > 2 || C == m_continuity)
        return;        

    m_continuity = C;
    applyContinuity();
}

void CompositeBezier2D::applyContinuity(){
    switch (m_continuity)
    {
        case 1 :
            applyContinuityC1();
            break;
        case 2 :
            applyContinuityC2();
            break;
    }
    for (Bezier2D * bezier : m_beziers){
        bezier->updateArclengthLUT();
    }
}

void CompositeBezier2D::applyContinuityC1(){
    if (m_beziers.size() < 2)
        return;
    for (int i = 1; i < m_beziers.size() - 1; ++i){
        if (m_breakContinuity[i] || m_breakContinuity[i - 1]) continue;
        Point::VectorType delta = m_beziers[i]->getP2() - m_beziers[i - 1]->getP1();
        delta /= 3;
        if (m_trajectoryExists[i])
            m_beziers[i - 1]->setP2(2 * m_beziers[i]->getP0()- m_beziers[i]->getP1());
        else{
            m_beziers[i]->setP1(m_beziers[i]->getP0() + 0.5 * delta);
            m_beziers[i - 1]->setP2(m_beziers[i]->getP0() - 0.5 * delta);
        }
    }
}

void CompositeBezier2D::applyContinuityC2(){
    applyContinuityC1();

    if (m_beziers.size() < 2)
        return;
    for (int i = 1; i < m_beziers.size(); ++i){
        m_beziers[i]->setP2(m_beziers[i - 1]->getP1() + 4 * (m_beziers[i - 1]->getP3() - m_beziers[i - 1]->getP2()));
    }

}

float CompositeBezier2D::sampleArcLength(float start, float end, int nbSamples, std::vector<Point::VectorType> &samples){
    float step = (end - start) / (nbSamples - 1);
    float s = start;
    for (int i = 0; i < nbSamples; i ++){
        samples.push_back(evalArcLength(s));
        s += step;
    }
    return step;
}

void CompositeBezier2D::replaceBezierCurve(Bezier2D * newCurve, float t, bool trajectoryEditable){
    static const float epsilon = 1e-6;
    for (int i = 0; i < m_times.size(); ++i){
        if (m_times[i] < t - epsilon || m_times[i] > t + epsilon) continue;
        m_beziers[i]->setP0(newCurve->getP0());
        m_beziers[i]->setP1(newCurve->getP1());
        m_beziers[i]->setP2(newCurve->getP2());
        m_beziers[i]->setP3(newCurve->getP3());
        m_trajectoryExists[i] = m_trajectoryExists[i] || !trajectoryEditable;
        applyContinuity();
        break;
    }
}