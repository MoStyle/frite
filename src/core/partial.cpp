#include "partial.h"
#include "vectorkeyframe.h"

#include <type_traits>

enum PARTIAL_TYPE { ORDER = 0, DRAWING };

static unsigned s_id = 0;

Partial::Partial(VectorKeyFrame *keyframe, double t) : m_keyframe(keyframe), m_id(s_id++), m_t(t) {

}

void Partial::setKeyframe(VectorKeyFrame *keyframe) {
    m_keyframe = keyframe;
}

// ******************************

OrderPartial::OrderPartial(VectorKeyFrame *keyframe, double t) 
    : Partial(keyframe, t),
      m_order(keyframe) {

}

OrderPartial::OrderPartial(VectorKeyFrame *keyframe, double t, const GroupOrder &order) 
    : Partial(keyframe, t),
      m_order(order) {
}

void OrderPartial::setKeyframe(VectorKeyFrame *keyframe) {
    Partial::setKeyframe(keyframe);
    m_order.setParentKeyFrame(keyframe);
}

bool OrderPartial::compare(const OrderPartial &other) const {
    return m_order.sameOrder(other.m_order);
}

void OrderPartial::load(QDomElement partialEl) {
    QDomElement groupOrderEl = partialEl.firstChildElement("group_order");
    m_order.load(groupOrderEl);
}

void OrderPartial::save(QDomElement &el) const {
    el.setAttribute("type", (int)ORDER);
    el.setAttribute("t", (double)m_t);
    QDomElement groupOrderEl = el.ownerDocument().createElement("group_order");
    m_order.save(groupOrderEl);
    el.appendChild(groupOrderEl);
}

void OrderPartial::debug() const {
    m_order.debug();    
}

// ******************************

DrawingPartial::DrawingPartial(VectorKeyFrame *keyframe, double t) 
    : Partial(keyframe, t),
      m_strokes() {
}

DrawingPartial::DrawingPartial(VectorKeyFrame *keyframe, double t, const StrokeIntervals &strokes) 
    : Partial(keyframe, t),
      m_strokes(strokes) {
}

bool DrawingPartial::compare(const DrawingPartial &other) const {
    return m_strokes.compare(other.m_strokes);
}

void DrawingPartial::load(QDomElement partialEl) {
    // QDomElement groupOrderEl = partialEl.firstChildElement("group_order");
    // m_order.load(groupOrderEl);
}

void DrawingPartial::save(QDomElement &el) const {
    // el.setAttribute("type", (int)DRAWING);
    // el.setAttribute("t", (double)m_t);
    // QDomElement groupOrderEl = el.ownerDocument().createElement("group_order");
    // m_order.save(groupOrderEl);
    // el.appendChild(groupOrderEl);
    // QDomElement strokesElt = el.ownerDocument().createElement("strokes");
    // // const StrokeIntervals &strokes = m_showInterStroke ? m_origin_strokes : m_strokes;
    // for (auto it = m_drawingPartials.firstPartial().strokes().constBegin(); it != m_drawingPartials.firstPartial().strokes().constEnd(); it++) {
    //     QDomElement strokeElt = doc.createElement("stroke");
    //     QString stringIntervals;
    //     QTextStream streamIntervals(&stringIntervals);
    //     strokesElt.appendChild(strokeElt);
    //     strokeElt.setAttribute("id", it.key());
    //     strokeElt.setAttribute("size", (unsigned int)it.value().size());
    //     for (Interval interval : it.value()) streamIntervals << interval.from() << " " << interval.to() << " ";
    //     QDomText txtIntervals = doc.createTextNode(stringIntervals);
    //     strokeElt.appendChild(txtIntervals);
    // }
    // el.appendChild(strokesElt);
}

// ******************************

template <class T>
Partials<T>::Partials(VectorKeyFrame *keyframe, T first) 
    : m_keyframe(keyframe) {
    static_assert(std::is_base_of<Partial, T>::value, "T not derived from Partial");
    if (keyframe != nullptr) insertPartial(first);
}

template <class T>
void Partials<T>::setKeyframe(VectorKeyFrame *keyframe) {
    m_keyframe = keyframe;
    for (auto it = m_partials.begin(); it != m_partials.end(); ++it) {
        it.value().setKeyframe(keyframe);
    }
}

template <class T>
bool Partials<T>::existsAfter(int inbetween, int stride) {
    double dt = 1.0 / stride;
    return lastPartialAt((inbetween + 1) * dt).t() >= inbetween * dt;
}

template <class T>
T &Partials<T>::lastPartialAt(double t) {
    t = std::clamp(t, 0.0, 1.0);
    auto it = m_partials.lowerBound(t);
    if (it == m_partials.begin()) {
        return it.value();
    }
    if (it != m_partials.end() && it.value().t() == t) {
        return it.value();
    }
    it = std::prev(it);
    return it.value();
}

template <class T>
const T &Partials<T>::constLastPartialAt(double t) const {
    t = std::clamp(t, 0.0, 1.0);
    auto it = m_partials.lowerBound(t);
    if (it != m_partials.end() && it.value().t() == t) {
        return it.value();
    }
    it = std::prev(it);
    return it.value();
}

template <class T>
T &Partials<T>::nextPartialAt(double t) {
    auto it = m_partials.upperBound(std::clamp(t, 0.0, 1.0));
    if (it == m_partials.end()) {
        VectorKeyFrame *next = m_keyframe->nextKeyframe();
        if (next == nullptr) {
            qCritical() << "Error in nextPartialAt(" << t << "): there is no next keyframe!";
        }
    }
    return it.value();
}

template <class T>
const T &Partials<T>::prevPartial(const T &partial) const {
    auto it = m_partials.find(partial.t());
    if (it != m_partials.begin()) {
        it = std::prev(it);
    }
    return it.value();
}

template <class T>
const T &Partials<T>::nextPartial(const T &partial) const {
    auto it = m_partials.find(partial.t());
    if (std::next(it) == m_partials.end()) {
        return it.value();
    }
    return std::next(it).value();
}

template <class T>
const T *Partials<T>::cpartial(unsigned int id) const {
    for (auto it = m_partials.constBegin(); it != m_partials.constEnd(); ++it) {
        if (it.value().id() == id) {
            return &(it.value());
        }
    }
    return nullptr;
}

template <class T>
T *Partials<T>::partial(unsigned int id)  {
    for (auto it = m_partials.begin(); it != m_partials.end(); ++it) {
        if (it.value().id() == id) {
            return &(it.value());
        }
    }
    return nullptr;
}

template <class T>
void Partials<T>::insertPartial(const T &partial) {
    m_partials.insert(partial.t(), partial);
}

template <class T>
void Partials<T>::removePartial(double t) {
    if (t == 0.0) {
        qCritical() << "Cannot remove partial at t=0.0";
        return;
    }
    m_partials.remove(t);
}

template <class T>
void Partials<T>::removeAfter(int inbetween, int stride) {
    double dt = 1.0 / stride;
    QMutableMapIterator<double, T> it(m_partials);
    while (it.hasNext()) {
        it.next();
        if (it.value().t() != 0.0 && it.value().t() >= inbetween * dt && it.value().t() <= (inbetween + 1) * dt) {
            it.remove();
        }
    }
}

template <class T>
void Partials<T>::movePartial(double tFrom, double tTo) {
    if (!m_partials.contains(tFrom)) return;
    T newPartial = T(lastPartialAt(tFrom));
    newPartial.setT(tTo);
    m_partials.insert(tTo, newPartial);
    m_partials.remove(tFrom);
}

template <class T>
void Partials<T>::set(const Partials<T> &other) {
    m_keyframe = other.m_keyframe;
    m_partials = other.m_partials;
    m_savedState = other.m_savedState;
}

/**
 * Ensure there is only one partial between 2 frames.
 * If multiple partials are between 2 frames only the last one is kept.
 */
template <class T>
void Partials<T>::syncWithFrames(int stride) {
    for (int inbetween = 0; inbetween < stride; ++inbetween) {
        T lastPartial = T(lastPartialAt((double)(inbetween + 1)/(double)stride));        
        removeAfter(inbetween, stride);
        if (lastPartial.t() <= ((double)inbetween/(double)stride)) continue;
        lastPartial.setT((inbetween + 0.5) / (double)stride);
        insertPartial(lastPartial);
    }
}

/**
 * Save the current partials 
 */
template <class T>
void Partials<T>::saveState() {
    m_savedState = m_partials;
}

/**
 * Restore the previously saved partials 
 */
template <class T>
void Partials<T>::restoreState() {
    if (m_savedState.size() == 0) {
        return;
    }
    m_partials = m_savedState;
}

/**
 * Clear the saved state 
 */
template <class T>
void Partials<T>::removeSavedState() {
    m_savedState.clear();
}

template <class T>
void Partials<T>::load(QDomElement &partialsEl) {
    QDomNode partialNode = partialsEl.firstChild();
    while (!partialNode.isNull()) {
        QDomElement partialEl = partialNode.toElement();
        int type = partialEl.attribute("type", "-1").toInt();
        double t = partialEl.attribute("t", "0.0").toDouble();
        switch (type) {
            case ORDER:
            {
                T partial(m_keyframe, t);
                partial.load(partialEl);
                insertPartial(partial);
                break;
            }
            default:
                qCritical() << "Couldn't load partial! Layer: " << m_keyframe->parentLayer()->id() << ", Keyframe: " << m_keyframe->keyframeNumber() << "type: " << type;
                break;
        }
        partialNode = partialNode.nextSibling();
    }
}

template <class T>
void Partials<T>::save(QDomDocument &doc, QDomElement &partialsEl) const {
    for (auto it = m_partials.constBegin(); it != m_partials.constEnd(); ++it) {
        QDomElement el = doc.createElement("partial");
        it.value().save(el);
        partialsEl.appendChild(el);
    }
}

template <class T>
void Partials<T>::debug() const {
    qDebug() << "Keyframe " << m_keyframe->keyframeNumber();
    qDebug() << "#partials = " << m_partials.size();
    qDebug() << "#partials in saved state = " << m_savedState.size();
    for (auto it = m_partials.constBegin(); it != m_partials.constEnd(); ++it) {
        std::cout << "Partial (t=" << it.key() << " | " << it.value().t() << "):" << std::endl;
        it.value().debug();
    }
}

template class Partials<OrderPartial>;
template class Partials<DrawingPartial>;