#ifndef __PARTIAL_H__
#define __PARTIAL_H__

#include "grouporder.h"
#include "strokeinterval.h"

#include <QDomElement>

class VectorKeyFrame;

/**
 * Store drawing information that can be "keyframed" between keyframes (i.e changes in group order)
 * Partial information is relative to the last keyframe (m_keyframe)
 */
class Partial {
public:
    Partial(VectorKeyFrame *keyframe, double t);

    VectorKeyFrame *keyframe() const { return m_keyframe; };

    unsigned int id() const { return m_id; }
    double t() const { return m_t; }
    void setT(double t) { m_t = t; }
    virtual void setKeyframe(VectorKeyFrame *keyframe);

    virtual void load(QDomElement partialEl) { }
    virtual void save(QDomElement &el) const { }

    void debug() const { }
protected:
    VectorKeyFrame *m_keyframe;
    unsigned int m_id;
    double m_t;
};

/**
 * Store a change in group order relative the the last keyframe
 */
class OrderPartial : public Partial {
public:
    OrderPartial(VectorKeyFrame *keyframe, double t);
    OrderPartial(VectorKeyFrame *keyframe, double t, const GroupOrder &order);

    void setKeyframe(VectorKeyFrame *keyframe) override;
    bool compare(const OrderPartial &other) const;

    GroupOrder &groupOrder() { return m_order; }
    const GroupOrder &groupOrder() const { return m_order; }

    void load(QDomElement partialEl) override;
    void save(QDomElement &el) const override;

    void debug() const;
private:
    GroupOrder m_order;
};

/**
 * Store a change in drawing 
 */
class DrawingPartial : public Partial {
public:
    DrawingPartial(VectorKeyFrame *keyframe, double t);
    DrawingPartial(VectorKeyFrame *keyframe, double t, const StrokeIntervals &order);

    bool compare(const DrawingPartial &other) const;

    StrokeIntervals &strokes() { return m_strokes; }
    const StrokeIntervals &strokes() const { return m_strokes; }

    void load(QDomElement partialEl) override;
    void save(QDomElement &el) const override;

private:
    StrokeIntervals m_strokes;
};

/**
 * Container for partial information
 */
template<class T>
class Partials {
public:
    Partials(VectorKeyFrame *keyframe, T first);

    int size() const { return m_partials.size(); };
    void setKeyframe(VectorKeyFrame *keyframe);
    bool exists(double t) const { return m_partials.contains(t); }
    bool existsAfter(int inbetween, int stride);

    const QMap<double, T> &partials() const { return m_partials; }
    QMap<double, T> &partials() { return m_partials; }
    
    T &lastPartialAt(double t);
    const T &constLastPartialAt(double t) const;
    T &nextPartialAt(double t);
    T &firstPartial() { return m_partials.first(); };
    const T &firstPartial() const { return m_partials.first(); };
    const T &prevPartial(const T &partial) const;
    const T &nextPartial(const T &partial) const;
    const T *cpartial(unsigned int id) const;
    T *partial(unsigned int id);

    void insertPartial(const T &partial);
    void removePartial(double t);
    void removeAfter(int inbetween, int stride);
    void movePartial(double tFrom, double tTo);
    void set(const Partials<T> &other);
    void removeIdenticalPartials();

    void syncWithFrames(int stride);

    void saveState();
    void restoreState();
    void removeSavedState();

    void load(QDomElement &partialsEl);
    void save(QDomDocument &doc, QDomElement &partialsEl) const;

    void debug() const;
private:
    VectorKeyFrame *m_keyframe;     // all partials are relative to the last keyframe
    QMap<double, T> m_partials;     // sorted map of partials (t -> partial)
    QMap<double, T> m_savedState;   
};

#endif // __PARTIAL_H__