#include "chartitem.h"
#include "editor.h"
#include "keyframedparams.h"
#include "group.h"
#include "vectorkeyframe.h"
#include "charttickitem.h"
#include "playbackmanager.h"
#include "layermanager.h"
#include "tabletcanvas.h"

#include "utils/utils.h"
#include "dialsandknobs.h"

static dkBool k_proxySmooth("Debug->Spacing->Proxy smoothing", false);
static dkSlider k_proxyStrength("ProxySpacing->Scale", 1, 1, 5, 1);

ChartItem::ChartItem(Editor *editor, VectorKeyFrame *keyframe, QPointF pos) : QGraphicsItem(), m_editor(editor) {
    m_pos = pos;
    m_length = 150;
    m_spacing = nullptr;
    m_nbTicks = 0;
    m_mode = GROUP;
    if (keyframe != nullptr)
        refresh(keyframe);
}

ChartItem::~ChartItem() {

}

QRectF ChartItem::boundingRect() const {
    return childrenBoundingRect();
}

unsigned int ChartItem::nbFixedTicks() const {
    return m_spacing->nbPoints();
}

/**
 * Construct the timing chart from the given keyframe and selected group(s)
 */
void ChartItem::refresh(VectorKeyFrame *keyframe) {
    // qDebug() << "Refreshing chart";

    // clear chart
    clearTicks();
    m_keyframe = keyframe;
    if (m_keyframe == nullptr) {
        hide();
        return;
    }
    m_spacing = nullptr;


    setChartMode(keyframe->selectedGroup(POST) != nullptr ? m_mode : KEY);

    setSpacingCurve();

    makeTicks();

    // qDebug() << "#items in chart: " << m_controlTicks.size();
}

/**
 * Update the spacing curve with respect to the current tick values
 */
void ChartItem::updateSpacing(int tickIdx, bool refreshAllTicks) {
    if (m_spacing == nullptr) {
        qCritical() << "ERROR: This chart is not associated with any spacing curve";
        return;
    }

    // update the animation curve 
    if (refreshAllTicks) {
        for (int i = 0; i < m_controlTicks.size(); ++i) {
            if (!m_controlTicks[i]->fixed()) {
                m_spacing->setKeyframe(Eigen::Vector2d((qreal)m_controlTicks[i]->idx() /  (qreal)(m_controlTicks.size() - 1), m_controlTicks[i]->xVal()), m_controlTicks[i]->elementIdx());
            }
        }   
    } else {
        m_spacing->setKeyframe(Eigen::Vector2d((qreal)tickIdx / (qreal)(m_controlTicks.size() - 1), m_controlTicks[tickIdx]->xVal()), m_controlTicks[tickIdx]->elementIdx());
    }

    // if multiple groups are selected, copy the new spacing curve into all selected groups
    if (m_mode == GROUP) {
        for (Group *group : m_keyframe->selection().selectedPostGroups()) {
            if (m_spacing->nbPoints() != group->spacing()->curve()->nbPoints()) qCritical() << "ERROR: groups have different spacing sampling";               
            for (int i = 0; i < m_spacing->nbPoints(); i++) {
                group->spacing()->curve()->setKeyframe(m_spacing->point(i), i);
            }
        }
    }

    synchronizeSpacingCurve(true, true);

    update();

    m_keyframe->makeInbetweensDirty();
}

/**
 * Update the spacing (animation curve and chart) using the ease in or out proxy 
 */
void ChartItem::updateSpacingProxy(ProxyMode mode) {    
    if (m_spacing == nullptr) {
        qCritical() << "ERROR: This chart is not associated with any spacing curve";
        return;
    }

    for (int i = 0; i < m_controlTicks.size(); ++i) {
        if (!m_controlTicks[i]->fixed()) {
            if (mode == INOROUT)
                m_controlTicks[i]->setXVal(Geom::easeInOrOut((qreal)m_controlTicks[i]->idx() /  (qreal)(m_controlTicks.size() - 1), -(m_proxyTicks[0]->xVal() * 2.0 - 1.0) * k_proxyStrength));
            else if (mode == INANDOUT) {
                m_controlTicks[i]->setXVal(Geom::easeInAndOut((qreal)m_controlTicks[i]->idx() /  (qreal)(m_controlTicks.size() - 1), -(m_proxyTicks[0]->xVal() * 2.0 - 1.0) * k_proxyStrength));
            }
            m_spacing->setKeyframe(Eigen::Vector2d((qreal)m_controlTicks[i]->idx() /  (qreal)(m_controlTicks.size() - 1), m_controlTicks[i]->xVal()), m_controlTicks[i]->elementIdx());
        }
    }

    // if multiple groups are selected, copy the new spacing curve into all selected groups
    if (m_mode == PROXY) {
        for (Group *group : m_keyframe->selection().selectedPostGroups()) {
            if (m_spacing->nbPoints() != group->spacing()->curve()->nbPoints()) qCritical() << "ERROR: groups have different spacing sampling";               
            for (int i = 0; i < m_spacing->nbPoints(); i++) {
                group->spacing()->curve()->setKeyframe(m_spacing->point(i), i);
            }
        }
    }

    synchronizeSpacingCurve(true, true);

    update();

    m_keyframe->makeInbetweensDirty();   
}

/**
 * Restore a linear spacing
 */
void ChartItem::resetControlTicks() {
    for (int i = 0; i < m_controlTicks.size(); ++i) {
        if (!m_controlTicks[i]->fixed()) {
            m_controlTicks[i]->setXVal((float)m_controlTicks[i]->idx() / (float)(m_controlTicks.size() - 1));
            m_controlTicks[i]->updatePos();
            // m_controlTicks[i]->update();
        }
    }
    updateSpacing(1, true);
}

void ChartItem::setChartMode(ChartMode mode) {
    m_mode = mode;
}

void ChartItem::setPos(QPointF pos) {
    m_pos = pos;
    for (ChartTickItem *item : m_controlTicks) {
        item->updatePos();
    }
    for (ChartTickItem *item : m_partialTicks) {
        item->updatePos();
    }
    for (ChartTickItem *item : m_proxyTicks) {
        item->updatePos();
    }
}

/**
 * Paint the main line of the chart. "Ticks" are drawn by their own items.
 */
void ChartItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QPen pen(Qt::black);
    QFontMetrics fontMetrics = QFontMetrics(m_editor->tabletCanvas()->canvasFont());

    float yOffset = ChartTickItem::HEIGHT / 2.0f;
    float xOffset = ChartTickItem::WIDTH / 2.0f;
    float xStart = xOffset + m_pos.x();
    pen.setWidth(2);
    painter->setPen(pen);

    int prevFrameNumber = m_keyframe->parentLayer()->getVectorKeyFramePosition(m_keyframe);
    int nextFrameNumber = m_keyframe->parentLayer()->getNextKeyFramePosition(prevFrameNumber);

    QString strPrevFrameNumber = QString("%1").arg(prevFrameNumber);
    QString strNextFrameNumber = QString("%1").arg(nextFrameNumber);
    QRect rectPrevFrameNumber = fontMetrics.tightBoundingRect(strPrevFrameNumber);
    QRect rectNextFrameNumber = fontMetrics.tightBoundingRect(strNextFrameNumber);
    int radiusPrevFrame = std::max(rectPrevFrameNumber.width(), rectPrevFrameNumber.height()) * 0.75;
    int radiusNextFrame = std::max(rectNextFrameNumber.width(), rectNextFrameNumber.height()) * 0.75;
    int radius = std::max(radiusNextFrame, radiusPrevFrame);

    QFont f = m_editor->tabletCanvas()->canvasFont();
    f.setPointSize(22);
    painter->setFont(f);
    painter->drawEllipse(QPointF(xStart, m_pos.y() - yOffset - radius), radius, radius);
    painter->drawEllipse(QPointF(xStart + m_length, m_pos.y() - yOffset - radius), radius, radius);
    painter->drawText(xStart - (rectPrevFrameNumber.width() * 0.5), m_pos.y() - yOffset - radius * 0.5, strPrevFrameNumber);
    painter->drawText(xStart + m_length - (rectNextFrameNumber.width() * 0.5), m_pos.y() - yOffset - radius * 0.5, strNextFrameNumber);

    // rectPrevFrameNumber.translate(QPoint(xStart - radius * 0.5, m_pos.y() - yOffset - radius * 0.5));
    // rectNextFrameNumber.translate(QPoint(xStart + m_length - radius * 0.5, m_pos.y() - yOffset - radius * 0.5));
    // painter->drawRect(rectPrevFrameNumber);
    // painter->drawRect(rectNextFrameNumber);

    painter->drawLine(xStart, m_pos.y() + yOffset, xStart + m_length, m_pos.y() + yOffset);
}

void ChartItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    event->accept();
}

void ChartItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (event->modifiers() & Qt::ShiftModifier) {
        QPointF delta = event->pos() - event->lastPos();
        setPos(m_pos + delta);
    }
}

void ChartItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    event->accept();
}

void ChartItem::wheelEvent(QGraphicsSceneWheelEvent *event) {
    int delta = event->delta();
    if (delta < 0 && m_length >= 20.0f) {
        m_length -= 10.0f;
        m_pos.setX(m_pos.x() + 5.0f);
    } else {
        m_length += 10.0f;
        m_pos.setX(m_pos.x() - 5.0f);
    }
    for (ChartTickItem *item : m_controlTicks) {
        item->updatePos();
    }
    for (ChartTickItem *item : m_partialTicks) {
        item->updatePos();
    }
    for (ChartTickItem *item : m_proxyTicks) {
        item->updatePos();
    }
    update();
    event->accept();
}

void ChartItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
    event->accept();
}

// put this in Group
void ChartItem::synchronizeSpacingCurve(bool withPrev, bool withNext) {
    if (!withNext && !withPrev) {
        return;
    }

    Layer *layer = m_editor->layers()->currentLayer();
    Group *group = m_keyframe->selectedGroup(POST);
    Group *nextGroup = nullptr;
    Group *prevGroup = nullptr;
    VectorKeyFrame *nextKey = nullptr;
    VectorKeyFrame *prevKey = nullptr;

    if (withNext) {
        nextKey = m_keyframe->nextKeyframe();
        nextGroup = group->nextPostGroup();
    }

    if (withPrev) {
        prevKey = m_keyframe->prevKeyframe();
        prevGroup = group->prevPostGroup();
    }

    if (nextGroup == nullptr && prevGroup == nullptr) {
        return;
    }

    unsigned int curCurveSize = group->spacing()->curve()->nbPoints();
    double curGroupMotionEnergy = group->motionEnergy();
    Bezier2D currentProxy;
    group->computeSpacingProxy(currentProxy);

    // TODO factorize withNext and withPrev
    if (nextGroup != nullptr && nextGroup->spacing()->curve()->nbPoints() >= 3) {
        Curve *nextCurve = nextGroup->spacing()->curve();
        unsigned int nextCurveSize = nextCurve->nbPoints();

        // Compute ticks relative position
        double base = nextCurve->point(1).y();
        double span = nextCurve->points().back().y() - base;
        std::vector<double> relativePos(nextCurveSize - 3);
        for (int i = 2; i < nextCurveSize - 1; ++i) {
            relativePos[i - 2] = (nextCurve->point(i).y() - base) / span;
        }

        // Set first tick constraint
        double nextGroupMotionEnergy = nextGroup->motionEnergy();
        Bezier2D nextProxy;
        nextGroup->computeSpacingProxy(nextProxy);
        Point::VectorType tangentOut = currentProxy.getP3() - currentProxy.getP2();
        // TODO: clamp if the new tangent goes out of bounds (unit square)
        Point::VectorType newTangent = tangentOut.cwiseProduct(Point::VectorType(((double)curCurveSize) / nextCurveSize, curGroupMotionEnergy / nextGroupMotionEnergy));
        nextProxy.setP1(nextProxy.getP0() + newTangent);
        Eigen::Vector2d nextSpacingFirstTick = nextCurve->point(1);
        double normalizedEnergy = std::clamp(nextProxy.evalYFromX(nextSpacingFirstTick.x()), 1e-5, 1.0 - 1e-5);
        nextCurve->setKeyframe(Eigen::Vector2d(nextSpacingFirstTick.x(), normalizedEnergy), 1);

        // Diffuse to next ticks
        if (!k_proxySmooth) {
            double newSpan = nextCurve->points().back().y() - normalizedEnergy;
            for (int i = 2; i < nextCurveSize - 1; ++i) {
                float tickX = nextCurve->point(i).x();
                nextCurve->setKeyframe(Eigen::Vector2d(tickX, std::min(normalizedEnergy + relativePos[i - 2] * newSpan, 1.0)), i);
            }
        } else {
            for (int i = 2; i < nextCurveSize - 1; ++i) {
                float tickX = nextCurve->point(i).x();
                nextCurve->setKeyframe(Eigen::Vector2d(tickX, std::min(nextProxy.evalYFromX(tickX), 1.0)), i);
            }        
        }

        nextKey->makeInbetweensDirty();
    }

    if (prevGroup != nullptr && prevGroup->spacing()->curve()->nbPoints() >= 3) {
        Curve *prevCurve = prevGroup->spacing()->curve();
        unsigned int prevCurveSize = prevCurve->nbPoints();

        // Set last tick constraint
        double prevGroupMotionEnergy = prevGroup->motionEnergy();
        Bezier2D prevProxy;
        prevGroup->computeSpacingProxy(prevProxy);
        Point::VectorType tangentIn = currentProxy.getP0() - currentProxy.getP1();
        // TODO: clamp if the new tangent goes out of bounds (unit square)
        Point::VectorType newTangent = tangentIn.cwiseProduct(Point::VectorType(((double)curCurveSize) / prevCurveSize, curGroupMotionEnergy / prevGroupMotionEnergy));
        prevProxy.setP2(prevProxy.getP3() + newTangent);
        Eigen::Vector2d prevSpacingLastTick = prevCurve->point(prevCurveSize - 2);
        double normalizedEnergy = std::clamp(prevProxy.evalYFromX(prevSpacingLastTick.x()), 1e-5, 1.0 - 1e-5);
        prevCurve->setKeyframe(Eigen::Vector2d(prevSpacingLastTick.x(), normalizedEnergy), prevCurveSize - 2);

        // Diffuse to next ticks
        for (int i = 1; i < prevCurveSize - 2; ++i) {
            float tickX = prevCurve->point(i).x();
            prevCurve->setKeyframe(Eigen::Vector2d(tickX, std::min(prevProxy.evalYFromX(tickX), 1.0)), i);
        }        

        prevKey->makeInbetweensDirty();
    }
}

/**
 * Fetch the spacing curve to edit with respect to the current keyframe, selected group(s) and chart mode. 
 */
void ChartItem::setSpacingCurve() {
    Curve *curve;
    switch (m_mode) {
        case KEY:
            curve = m_keyframe->spacing()->curve();
            hide();
            break;
        case GROUP:
        case PARTIAL:
        case PROXY:
            show();
            if (m_keyframe->selectedGroup(POST) == nullptr) {
                setChartMode(KEY);
                setSpacingCurve();
                return;
            }
            curve = m_keyframe->selectedGroup(POST)->spacing()->curve();
            break;
        default:
            qCritical() << "ERROR in setSpacingCurve(): invalid chart mode";
            break;
    }
    if (curve->interpType() != Curve::MONOTONIC_CUBIC_INTERP) {
        qCritical() << "ERROR in setSpacingCurve(): invalid spacing curve type";
    }
    m_spacing = curve;
}

/**
 * Instanciate all ticks with respect to the current keyframe exposure (same number of ticks as inbetweens) and spacing. 
 */
void ChartItem::makeTicks(unsigned int nbTicks) {
    clearTicks();

    // resample spacing curve if there isn't as many control points as frame
    // yes we may lose information
    int inbetweens = m_keyframe->parentLayer()->stride(m_keyframe->parentLayer()->getVectorKeyFramePosition(m_keyframe)) - 1;
    if (inbetweens < 0) return;
    if (m_spacing->nbPoints() - 2 != inbetweens) {
        m_spacing->resample(inbetweens);
    }
    m_nbTicks = m_spacing->nbPoints();

    for (int i = 0; i < m_spacing->nbPoints(); ++i) {
        bool fixed = i == 0 || i == m_spacing->nbPoints() - 1;
        m_controlTicks.append(new ChartTickItem(this, ChartTickItem::CONTROL, i, m_pos.x(), m_pos.y(), m_spacing->point(i).y(), i, fixed));
        m_controlTicks.back()->setParentItem(this);
    }

    if (m_mode == PARTIAL) {
        int i = 0;
        for (auto it = m_keyframe->orderPartials().partials().constBegin(); it != m_keyframe->orderPartials().partials().constEnd(); ++it) {
            if (it.key() == 0.0) continue;
            m_partialTicks.append(new ChartTickItem(this, ChartTickItem::TICKORDERPARTIAL, i, m_pos.x(), m_pos.y(), m_spacing->evalAt(it.key()), it.value().id(), false));
            m_partialTicks.back()->setParentItem(this); 
            ++i;           
        }
        Group *group = m_keyframe->selectedGroup(POST);
        for (auto it = group->drawingPartials().partials().constBegin(); it != group->drawingPartials().partials().constEnd(); ++it) {
            if (it.key() == 0.0) continue;
            m_partialTicks.append(new ChartTickItem(this, ChartTickItem::TICKDRAWINGPARTIAL, i, m_pos.x(), m_pos.y(), m_spacing->evalAt(it.key()), it.value().id(), false));
            m_partialTicks.back()->setParentItem(this); 
            ++i;           
        }
    } else if (m_mode == PROXY) {
        m_proxyTicks.append(new ChartTickItem(this, ChartTickItem::TICKPROXY, 0, m_pos.x(), m_pos.y(), 0.5, 0, false));
        m_proxyTicks.back()->setParentItem(this); 
    }
}

void ChartItem::clearTicks() {
    for (ChartTickItem *item : m_controlTicks) {
        item->setParentItem(nullptr);
    }
    for (ChartTickItem *item : m_partialTicks) {
        item->setParentItem(nullptr);
    }
    for (ChartTickItem *item : m_proxyTicks) {
        item->setParentItem(nullptr);
    }
    qDeleteAll(m_controlTicks);
    qDeleteAll(m_partialTicks);
    qDeleteAll(m_proxyTicks);
    m_controlTicks.resize(0);
    m_partialTicks.resize(0);
    m_proxyTicks.resize(0);
}
