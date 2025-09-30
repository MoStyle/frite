/*
 * SPDX-FileCopyrightText: 2013 Romain Vergne <romain.vergne@inria.fr>
 * SPDX-FileCopyrightText: 2020-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KEYFRAMED_PARAMS
#define KEYFRAMED_PARAMS

#include <QTransform>

#include "animationcurve.h"
#include "point.h"

#include <QDomElement>
#include <QTextStream>
#include <set>

class KeyFrame;

class KeyframedVar {
   public:
    KeyframedVar(const QString &name);
    KeyframedVar(const KeyframedVar &other);
    KeyframedVar(const KeyframedVar &other, int i, int j);
    virtual ~KeyframedVar();

    inline QString name() const { return _name; }
    inline unsigned int nbCurves() const { return static_cast<unsigned int>(_curves.size()); }

    inline Curve *curve(unsigned int i = 0) const {
        assert(i < nbCurves());
        return _curves[i];
    }
    void setInterpolation(QString nodeName, int interpolation);
    void resetTangent();
    void scaleTangentVertical(double factor);
    inline virtual QString curveName(unsigned int i = 0) const { return QString::number(i); }

    virtual void moveKeys(QString nodeName, int offsetFirst, int offsetLast) = 0;
    virtual void removeKeyBefore(QString nodeName, double atFrame) = 0;
    virtual void removeKeyAfter(QString nodeName, double atFrame) = 0;
    virtual void removeKeys(QString nodeName) = 0;
    virtual int addKey(QString nodeName, double atFrame) = 0;
    virtual void normalizeX();
    inline virtual void removeLastPoint() {
        for (size_t i = 0; i < nbCurves(); i++) _curves[i]->removeLastPoint();
    }

    virtual void save(QDomDocument &doc, QDomElement &transformation) const = 0;
    virtual void load(QDomElement &transformation) = 0;
    virtual void print(std::ostream &os);

   protected:
    QString _name;
    std::vector<Curve *> _curves;

   public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

class KeyframedReal : public KeyframedVar {
   public:
    KeyframedReal(const QString &name, double minVal = 0.0, double maxVal = 1.0, double defaultVal = 0.0);
    KeyframedReal(const KeyframedReal &other, int i, int j);

    QString name() { return _name; }

    inline double get() const { return _currentVal; }
    inline void set(double v) { _currentVal = v; };

    inline bool frameChanged(double x) {
        const double tmp = curve()->evalAt(x);
        // report that the new frame did not change the current variable
        if (_currentVal == tmp) return false;
        _currentVal = tmp;
        return true;
    }

    virtual void moveKeys(QString nodeName, int offsetFirst, int offsetLast);
    virtual void removeKeyBefore(QString nodeName, double atFrame);
    virtual void removeKeyAfter(QString nodeName, double atFrame);
    virtual void removeKeys(QString nodeName);
    virtual int addKey(QString nodeName, double atFrame);

    void save(QDomDocument &doc, QDomElement &transformation) const;
    void load(QDomElement &transformation);

   private:
    double _minVal;
    double _maxVal;
    double _currentVal;

   public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

class KeyframedVector : public KeyframedVar {
   public:
    KeyframedVector(const QString &name, const Point::VectorType &defaultVal = Point::VectorType::Zero());
    KeyframedVector(const KeyframedVector &other, int i, int j);

    virtual ~KeyframedVector() {}


    inline Point::VectorType get() const { return _currentVal; }
    inline Point::VectorType getDerivative() const { return _currentDer; } 
    inline void set(const Point::VectorType &v) { _currentVal = v; }

    inline bool frameChanged(double x) {
        const Point::VectorType tmp(curve(0)->evalAt(x), curve(1)->evalAt(x));
        const Point::VectorType tmpDer(curve(0)->evalDerivativeAt(x), curve(1)->evalDerivativeAt(x));
        // report that the new frame did not change the current variable
        if (_currentVal == tmp && _currentDer == tmpDer) return false;
        _currentVal = tmp;
        _currentDer = tmpDer;
        return true;
    }

    virtual void moveKeys(QString nodeName, int offsetFirst, int offsetLast);
    virtual void removeKeyBefore(QString nodeName, double atFrame);
    virtual void removeKeyAfter(QString nodeName, double atFrame);
    virtual void removeKeys(QString nodeName);
    virtual int addKey(QString nodeName, double atFrame);
    void keys(std::vector<double> &keys);

    void save(QDomDocument &doc, QDomElement &transformation) const;
    void load(QDomElement &transformation);

    inline QString curveName(unsigned int i = 0) const {
        switch (i) {
            case 0:
                return "X";
            case 1:
                return "Y";
            default:
                return KeyframedVar::curveName(i);
        }
    }

   private:
    Point::VectorType _currentVal;
    Point::VectorType _currentDer;

   public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

#define MAXRANGE 99999

class KeyframedTransform {
   public:
    KeyframedTransform(QString nodeName)
        : translation(KeyframedVector("Translation")),
          scaling(KeyframedVector("Scaling", Point::VectorType(1, 1))),
          rotation(KeyframedReal("Rotation", -MAXRANGE, MAXRANGE, 0.0)),
          m_nodeName(nodeName) {
        translation.setInterpolation(m_nodeName, Curve::HERMITE_INTERP);
        scaling.setInterpolation(m_nodeName, Curve::HERMITE_INTERP);
        rotation.setInterpolation(m_nodeName, Curve::HERMITE_INTERP);
    }

    KeyframedTransform(const KeyframedTransform &other, int i, int j)
        : translation(KeyframedVector(other.translation, i, j)),
          scaling(KeyframedVector(other.scaling, i, j)),
          rotation(KeyframedReal(other.rotation, i, j)),
          m_nodeName(other.m_nodeName) {}

    inline void frameChanged(double time) {
        translation.frameChanged(time);
        scaling.frameChanged(time);
        rotation.frameChanged(time);
    }

    inline int addKeys(double atFrame = -1) {
        translation.addKey(m_nodeName, atFrame);
        scaling.addKey(m_nodeName, atFrame);
        return rotation.addKey(m_nodeName, atFrame);
    }

    inline void moveKeys(int offsetFirst, int offsetLast) {
        translation.moveKeys(m_nodeName, offsetFirst, offsetLast);
        scaling.moveKeys(m_nodeName, offsetFirst, offsetLast);
        rotation.moveKeys(m_nodeName, offsetFirst, offsetLast);
    }

    inline void setAt(double frame) {
        translation.set(Point::VectorType(translation.curve(0)->evalAt(frame), translation.curve(1)->evalAt(frame)));
        scaling.set(Point::VectorType(scaling.curve(0)->evalAt(frame), scaling.curve(1)->evalAt(frame)));
        rotation.set(rotation.curve()->evalAt(frame));
    }

    inline void removeKeyAfter(double atFrame) {
        translation.removeKeyAfter(m_nodeName, atFrame);
        scaling.removeKeyAfter(m_nodeName, atFrame);
        rotation.removeKeyAfter(m_nodeName, atFrame);
    }

    inline void removeLastPoint() {
        translation.removeLastPoint();
        scaling.removeLastPoint();
        rotation.removeLastPoint();
    }

    inline void resetTangents() {
        translation.resetTangent();
        scaling.resetTangent();
        rotation.resetTangent();
    }

    inline void scaleTangentVertical(double factor = 1.0) {
        translation.scaleTangentVertical(factor);
        scaling.scaleTangentVertical(factor);
        rotation.scaleTangentVertical(factor);
    }

    // split the KeyframedTransform at the given time and returns the second half
    // the KeyframedTransformed is truncated to be the first half
    // if scale is true then both have are normalized
    KeyframedTransform *split(double time, bool scale=true);

    void keys(std::set<double> &keys);

    void save(QDomDocument &doc, QDomElement &transformation) const;
    void load(QDomElement &transformation);

    inline void print(std::ostream &os) {
        translation.print(os);
        scaling.print(os);
        rotation.print(os);
    }

    KeyframedVector translation;
    KeyframedVector scaling;
    KeyframedReal rotation;

   private:
    // KeyFrame *_keyFrame;
    QString m_nodeName;
};

#endif  // KEYFRAMED_PARAMS
