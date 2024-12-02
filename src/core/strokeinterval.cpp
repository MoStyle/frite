#include "strokeinterval.h"

#include <QtGlobal>

#include "stroke.h"
#include "lattice.h"
#include "vectorkeyframe.h"

// INTERVALS

void Intervals::append(const Interval &interval) {
    if (empty()) {
        QList<Interval>::append(interval);
        m_nbPoints += interval.nbPoints();
        return;
    }

    Interval newInterval = interval;

    // go through the list and merge the new interval with every interval it intersects with and remove them
    QMutableListIterator<Interval> it(*this);
    while (it.hasNext()) {
        Interval &nextInterval = it.next();
        // qDebug() << "does " << interval.from() << ", " << interval.to() << "  intersects " << nextInterval.from() << ", " << nextInterval.to() << "   = " << newInterval.intersects(nextInterval);
        if (newInterval.intersects(nextInterval) || newInterval.connected(nextInterval)) {
            newInterval.merge(nextInterval);
            m_nbPoints -= nextInterval.nbPoints();
            it.remove();
        }
    }

    // add the new interval
    QList<Interval>::append(newInterval);
    m_nbPoints += newInterval.nbPoints();
}

void Intervals::append(const Intervals &intervals) {
    if (empty()) {
        QList<Interval>::append(intervals);
        m_nbPoints += intervals.nbPoints();
        return;
    }

    for (const Interval &interval : intervals) {
        append(interval);
    }
}

bool Intervals::compare(const Intervals &other) const {
    if (size() != other.size()) return false;
    for (unsigned int i = 0; i < size(); ++i) {
        if (!at(i).compare(other.at(i))) return false;
    }
    return true;
}

bool Intervals::containsPoint(unsigned int idx) const {
    for (unsigned int i = 0; i < size(); ++i) {
        if (at(i).contains(idx)) return true;
    }
    return false;
}

// STROKEINTERVALS

bool StrokeIntervals::compare(const StrokeIntervals &other) const {
    if (size() != other.size()) return false;
    for (auto it = cbegin(); it != cend(); ++it) {
        if (!other.contains(it.key()) || !it.value().compare(other.value(it.key()))) return false;
    }
    return true;
}

bool StrokeIntervals::containsPoint(unsigned int strokeId, unsigned int pointIdx) const {
    if (!contains(strokeId)) return false;
    return value(strokeId).containsPoint(pointIdx);
}

void StrokeIntervals::forEachPoint(const VectorKeyFrame *key, std::function<void(Point *)> func, unsigned int id) const {
    Stroke *stroke = key->stroke(id); 
    const Intervals &intervals = value(id);
    for (const Interval &interval : intervals) {
        for (unsigned int i = interval.from(); i <= interval.to(); i++) {
            func(stroke->points()[i]);
        }
    }
}

void StrokeIntervals::forEachPoint(const VectorKeyFrame *key, std::function<void(Point *)> func) const {
    for (auto it = begin(); it != end(); ++it) {
        forEachPoint(key, func, it.key());
    }
}

void StrokeIntervals::forEachPoint(const VectorKeyFrame *key, std::function<void(Point *, unsigned int sId, unsigned int pId)> func, unsigned int id) const {
    Stroke *stroke = key->stroke(id); 
    const Intervals &intervals = value(id);
    for (const Interval &interval : intervals) {
        for (unsigned int i = interval.from(); i <= interval.to(); i++) {
            func(stroke->points()[i], id, i);
        }
    }
}

void StrokeIntervals::forEachPoint(const VectorKeyFrame *key, std::function<void(Point *, unsigned int sId, unsigned int pId)> func) const {
    for (auto it = begin(); it != end(); ++it) {
        forEachPoint(key, func, it.key());
    }
}

void StrokeIntervals::forEachInterval(std::function<void(const Interval &)> func) const {
    for (auto it = constBegin(); it != constEnd(); ++it) {
        const Intervals &intervals = it.value();
        for (const Interval &interval : intervals) {
            func(interval);
        }
    }
}

void StrokeIntervals::forEachInterval(std::function<void(const Interval &, unsigned int sId)> func) const {
    for (auto it = constBegin(); it != constEnd(); ++it) {
        const Intervals &intervals = it.value();
        for (const Interval &interval : intervals) {
            func(interval, it.key());
        }
    }
}
