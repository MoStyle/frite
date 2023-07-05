/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

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
        if (newInterval.intersects(nextInterval)) {
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

// STROKEINTERVALS 

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
