/*
 * SPDX-FileCopyrightText: 2013 Romain Vergne <romain.vergne@inria.fr>
 * SPDX-FileCopyrightText: 2020-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: MPL-2.0
 */

#include "keyframedparams.h"
#include "animationcurve.h"
#include <QDebug>

KeyframedVar::KeyframedVar(const QString &name) : _name(name) {}

KeyframedVar::KeyframedVar(const KeyframedVar &other) {
    _name = other._name;
    for (unsigned int i = 0; i < other.nbCurves(); ++i) {
        _curves.push_back(new Curve(*other._curves[i]));
    }
}

// i and j inclusive
KeyframedVar::KeyframedVar(const KeyframedVar &other, int i, int j) {
    _name = other._name;
    for (unsigned int k = 0; k < other.nbCurves(); ++k) {
        _curves.push_back(other._curves[k]->cut(i, j));
    }
}

KeyframedVar::~KeyframedVar() {
    for (unsigned int i = 0; i < nbCurves(); ++i) {
        delete _curves[i];
    }
    _curves.clear();
}

void KeyframedVar::setInterpolation(QString nodeName, int interpolation) {
    for (Curve *curve : _curves) curve->setInterpolation(interpolation);
}

void KeyframedVar::resetTangent() {
    for (Curve *curve : _curves) curve->setPiecewiseLinear();
}

void KeyframedVar::scaleTangentVertical(double factor) {
    for (Curve *curve : _curves) curve->scaleTangentVertical(factor);
}

void KeyframedVar::normalizeX() {
    for (Curve *curve : _curves) curve->normalizeX();
}

void KeyframedVar::print(std::ostream &os) { 
    int idx = 0;
    for (Curve *curve : _curves) {
        os << "Curve " << idx << std::endl;
        curve->print(os); 
        ++idx;
    } 
}

KeyframedReal::KeyframedReal(const QString &name, double minVal, double maxVal, double defaultVal)
    : KeyframedVar(name), _minVal(minVal), _maxVal(maxVal), _currentVal(defaultVal) {
    _curves.push_back(new Curve(Eigen::Vector2d(1.0, _currentVal)));
}

KeyframedReal::KeyframedReal(const KeyframedReal &other, int i, int j)
    : KeyframedVar(other, i, j), _minVal(other._minVal), _maxVal(other._maxVal), _currentVal(other._currentVal) {}


void KeyframedReal::moveKeys(QString nodeName, int offsetFirst, int offsetLast) {
    curve()->moveKeys(offsetFirst, offsetLast);
}

void KeyframedReal::removeKeyBefore(QString nodeName, double atFrame) {
    curve()->removeKeyframeBefore(atFrame);
}

void KeyframedReal::removeKeyAfter(QString nodeName, double atFrame) {
    curve()->removeKeyframeAfter(atFrame);
}

void KeyframedReal::removeKeys(QString nodeName) {
    curve()->removeKeys();
}

int KeyframedReal::addKey(QString nodeName, double atFrame) {
    double current = atFrame;
    set(_currentVal);
    int idx = curve()->addKeyframe(Eigen::Vector2d(current, get()));
    return idx;
}

void KeyframedReal::save(QDomDocument &doc, QDomElement &transformation) const {
    transformation.setAttribute("interpType", curve()->interpType());
    QDomElement interp_points = doc.createElement("interp_points");
    QDomElement interp_tangents = doc.createElement("interp_tangents");

    interp_points.setAttribute("size", curve()->nbPoints());
    interp_tangents.setAttribute("size", curve()->nbTangents());

    QString string;
    QTextStream stream(&string);

    for (size_t i = 0; i < curve()->nbPoints(); ++i) stream << curve()->point(i)[0] << " " << curve()->point(i)[1] << " ";
    QDomText txt = doc.createTextNode(string);
    interp_points.appendChild(txt);

    stream.flush();
    string.clear();

    for (size_t i = 0; i < curve()->nbTangents(); ++i)
        stream << curve()->tangent(i)[0] << " " << curve()->tangent(i)[1] << " " << curve()->tangent(i)[2] << " " << curve()->tangent(i)[3]
               << " ";
    txt = doc.createTextNode(string);
    interp_tangents.appendChild(txt);

    transformation.appendChild(interp_points);
    transformation.appendChild(interp_tangents);

    if (curve()->interpType() == Curve::MONOTONIC_CUBIC_INTERP) {
        CubicMonotonicInterpolator *interp = dynamic_cast<CubicMonotonicInterpolator *>(curve()->interpolator());
        QDomElement interp_gradients = doc.createElement("interp_gradients");
        interp_gradients.setAttribute("size", interp->nbSlopes());
        stream.flush();
        string.clear();
        for (unsigned int i = 0; i < interp->nbSlopes(); ++i) {
            stream << interp->slopeAt(i) << " ";
        }
        txt = doc.createTextNode(string);
        interp_gradients.appendChild(txt);
        transformation.appendChild(interp_gradients);
    }
}

void KeyframedReal::load(QDomElement &transformation) {
    removeKeys(name());
    curve()->setInterpolation(transformation.attribute("interpType").toInt());

    QDomElement domPoints = transformation.firstChildElement();
    QDomElement domTangents = domPoints.nextSiblingElement();

    QString string = domPoints.text();
    QTextStream stream(&string);
    double x, y;
    for (int i = 0; i < domPoints.attribute("size").toInt(); ++i) {
        stream >> x >> y;
        curve()->addKeyframe(Eigen::Vector2d(x, y));
    }

    string = domTangents.text();
    stream.setString(&string);
    for (int i = 0; i < domTangents.attribute("size").toInt(); ++i) {
        double x1, y1, x2, y2;
        stream >> x1 >> y1 >> x2 >> y2;
        curve()->setTangent(Eigen::Vector4d(x1, y1, x2, y2), i);
    }

    if (curve()->interpType() == Curve::MONOTONIC_CUBIC_INTERP) {
        QDomElement domGradients = domTangents.nextSiblingElement();
        string = domGradients.text();
        stream.setString(&string);
        double g = 0.0;
        CubicMonotonicInterpolator *interp = dynamic_cast<CubicMonotonicInterpolator *>(curve()->interpolator());
        for (int i = 0; i < domGradients.attribute("size").toInt(); ++i) {
            stream >> g;
            interp->setSlope(i, g);
        }
    }
}

KeyframedVector::KeyframedVector(const KeyframedVector &other, int i, int j) : KeyframedVar(other, i, j), _currentVal(other._currentVal) {}

KeyframedVector::KeyframedVector(const QString &name, const Point::VectorType &defaultVal) : KeyframedVar(name), _currentVal(defaultVal) {
    _curves.push_back(new Curve(Eigen::Vector2d(1.0, _currentVal[0])));
    _curves.push_back(new Curve(Eigen::Vector2d(1.0, _currentVal[1])));
}

void KeyframedVector::moveKeys(QString nodeName, int offsetFirst, int offsetLast) {
    curve(0)->moveKeys(offsetFirst, offsetLast);
    curve(1)->moveKeys(offsetFirst, offsetLast);
}

void KeyframedVector::removeKeyBefore(QString nodeName, double atFrame) {
    curve(0)->removeKeyframeBefore(atFrame);
    curve(1)->removeKeyframeBefore(atFrame);
}

void KeyframedVector::removeKeyAfter(QString nodeName, double atFrame) {
    curve(0)->removeKeyframeAfter(atFrame);
    curve(1)->removeKeyframeAfter(atFrame);
}

void KeyframedVector::removeKeys(QString nodeName) {
    curve(0)->removeKeys();
    curve(1)->removeKeys();
}

int KeyframedVector::addKey(QString nodeName, double atFrame) {
    set(_currentVal);
    int idx = curve(0)->addKeyframe(Eigen::Vector2d(atFrame, get()[0]));
    curve(1)->addKeyframe(Eigen::Vector2d(atFrame, get()[1]));
    return idx;
}

void KeyframedVector::keys(std::vector<double> &keys) {
    assert(curve(0)->nbPoints() == curve(1)->nbPoints());
    for (size_t i = 0; i < curve(0)->nbPoints(); i++) {
        keys.push_back(curve(0)->point(i).x());
        if (curve(1)->point(i).x() != keys.back())
            qCritical() << "x and y translation curves don't have the name number of control points !";
    }
}

void KeyframedVector::save(QDomDocument &doc, QDomElement &transformation) const {
    for (size_t c = 0; c < nbCurves(); ++c) {
        QDomElement sc = doc.createElement(curveName(c));
        sc.setAttribute("interpType", curve(c)->interpType());
        QDomElement interp_points = doc.createElement("interp_points");
        QDomElement interp_tangents = doc.createElement("interp_tangents");

        interp_points.setAttribute("size", curve(c)->nbPoints());
        interp_tangents.setAttribute("size", curve(c)->nbTangents());

        QString string;
        QTextStream stream(&string);

        for (size_t i = 0; i < curve(c)->nbPoints(); ++i) stream << curve(c)->point(i)[0] << " " << curve(c)->point(i)[1] << " ";
        QDomText txt = doc.createTextNode(string);
        interp_points.appendChild(txt);

        stream.flush();
        string.clear();

        for (size_t i = 0; i < curve(c)->nbTangents(); ++i)
            stream << curve(c)->tangent(i)[0] << " " << curve(c)->tangent(i)[1] << " " << curve(c)->tangent(i)[2] << " "
                   << curve(c)->tangent(i)[3] << " ";
        txt = doc.createTextNode(string);
        interp_tangents.appendChild(txt);

        sc.appendChild(interp_points);
        sc.appendChild(interp_tangents);
        transformation.appendChild(sc);
    }
}

void KeyframedVector::load(QDomElement &transformation) {
    removeKeys(name());
    for (size_t c = 0; c < nbCurves(); ++c) {
        QDomElement trans = transformation.firstChildElement(curveName(c));
        curve(c)->setInterpolation(trans.attribute("interpType").toInt());

        QDomElement domPoints = trans.firstChildElement();
        QDomElement domTangents = domPoints.nextSiblingElement();

        QString string = domPoints.text();
        QTextStream stream(&string);
        double x, y;
        for (int i = 0; i < domPoints.attribute("size").toInt(); ++i) {
            stream >> x >> y;
            curve(c)->addKeyframe(Eigen::Vector2d(x, y));
        }

        string = domTangents.text();
        stream.setString(&string);
        for (int i = 0; i < domTangents.attribute("size").toInt(); ++i) {
            double x1, y1, x2, y2;
            stream >> x1 >> y1 >> x2 >> y2;
            curve(c)->setTangent(Eigen::Vector4d(x1, y1, x2, y2), i);
        }
    }
}

KeyframedTransform *KeyframedTransform::split(double time, bool scale) {
    frameChanged(time);
    int idx = addKeys(time);
    int nbPoints = translation.curve()->nbPoints();

    // create a new KeyframedTransform from idx to the last control point
    KeyframedTransform *secondHalf = new KeyframedTransform(*this, idx, nbPoints - 1);
    int nbPointsSecondHalf = secondHalf->translation.curve()->nbPoints();

    // truncate the current keyframedtransform up to idx
    while (translation.curve()->nbPoints() > idx + 1) {
        removeLastPoint();
    }

    // rescale y component for the second half of the curve (since it represents a relative value)
    // reminder: all curves in the transform have the same number of keys (i.e control points) and they are
    // all synchronized in time (x component)
    secondHalf->frameChanged(0.0);
    double currentRotation, rotationOffset = secondHalf->rotation.get();
    Point::VectorType currentTranslation, currentScaling, 
                      translationOffset = secondHalf->translation.get(), 
                      scalingOffset = secondHalf->scaling.get();
                      
    for (int i = 0; i < nbPointsSecondHalf; ++i) {
        double x = secondHalf->translation.curve()->point(i)[0];

        secondHalf->translation.frameChanged(x);
        secondHalf->rotation.frameChanged(x);
        secondHalf->scaling.frameChanged(x);

        currentTranslation = secondHalf->translation.get() - translationOffset;
        currentRotation = secondHalf->rotation.get() - rotationOffset;
        currentScaling = secondHalf->scaling.get() - scalingOffset + Point::VectorType::Ones();

        secondHalf->translation.set(x == 0.0 ? Point::VectorType::Zero() : currentTranslation);
        secondHalf->rotation.set(x == 0.0 ? 0.0 : currentRotation);
        secondHalf->scaling.set(x == 0.0 ? Point::VectorType::Ones() : currentScaling);

        secondHalf->addKeys(x);
    }

    return secondHalf;
}

void KeyframedTransform::keys(std::set<double> &keys) {
    auto addVarKeys = [&keys](KeyframedVar *var) {
        for (size_t i = 0; i < var->nbCurves(); i++) {
            Curve *curve = var->curve(i);
            for (size_t j = 0; j < curve->nbPoints(); j++) {
                keys.insert(curve->point(j).x());
            }
        }
    };
    addVarKeys(&translation);
    addVarKeys(&scaling);
    addVarKeys(&rotation);
}

void KeyframedTransform::save(QDomDocument &doc, QDomElement &transformation) const {
    QDomElement elt = doc.createElement("translation");
    translation.save(doc, elt);
    transformation.appendChild(elt);

    elt = doc.createElement("rotation");
    rotation.save(doc, elt);
    transformation.appendChild(elt);

    elt = doc.createElement("scaling");
    scaling.save(doc, elt);
    transformation.appendChild(elt);
}

void KeyframedTransform::load(QDomElement &transformation) {
    QDomElement elt = transformation.firstChildElement("translation");
    if (!elt.isNull()) translation.load(elt);
    elt = elt.nextSiblingElement("rotation");
    if (!elt.isNull()) rotation.load(elt);
    elt = elt.nextSiblingElement("scaling");
    if (!elt.isNull()) scaling.load(elt);
}
