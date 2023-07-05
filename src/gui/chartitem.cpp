/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "chartitem.h"
#include "editor.h"
#include "keyframedparams.h"
#include "group.h"
#include "vectorkeyframe.h"
#include "charttickitem.h"
#include "playbackmanager.h"
#include "layermanager.h"
#include "canvasscenemanager.h"
#include "utils/utils.h"
#include "dialsandknobs.h"

static dkBool k_proxySmooth("Debug->Spacing->Proxy smoothing", false);

ChartItem::ChartItem(Editor *editor, VectorKeyFrame *keyframe, QPointF pos) : QGraphicsItem(), m_editor(editor) {
    m_pos = pos;
    m_length = 150;
    m_spacing = nullptr;
    m_nbTicks = 0;
    m_mode = KEY;
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

    if (keyframe == nullptr) return;

    setChartMode(keyframe->selectedGroup(POST) != nullptr ? GROUP : KEY);

    setSpacingCurve();

    makeTicks();

    qDebug() << "#items in chart: " << m_controlTicks.size();
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
                m_spacing->setKeyframe(Eigen::Vector2f((float)m_controlTicks[i]->idx() /  (float)(m_controlTicks.size() - 1), m_controlTicks[i]->xVal()), (unsigned int)(m_controlTicks[i]->pointIdx()));
            }
        }   
    } else {
        m_spacing->setKeyframe(Eigen::Vector2f((float)tickIdx / (float)(m_controlTicks.size() - 1), m_controlTicks[tickIdx]->xVal()), (unsigned int)(m_controlTicks[tickIdx]->pointIdx()));
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

    m_editor->scene()->spacingChanged();
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
    if (m_keyframe == nullptr) return;
    m_mode = mode;
}

void ChartItem::setPos(QPointF pos) {
    m_pos = pos;
    for (ChartTickItem *item : m_controlTicks) {
        item->updatePos();
    }
}

/**
 * Paint the main line of the chart. "Ticks" are drawn by their own items.
 */
void ChartItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QPen pen(Qt::black);
    pen.setWidth(2);
    painter->setPen(pen);
    float yOffset = ChartTickItem::HEIGHT / 2.0f;
    float xOffset = ChartTickItem::WIDTH / 2.0f;
    float xStart = xOffset + m_pos.x();
    painter->drawLine(xStart, m_pos.y() + yOffset, xStart + m_length, m_pos.y() + yOffset);

    pen.setWidthF(1.0f);
    pen.setColor(QColor(200, 16, 16));
    painter->setPen(pen);
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
    qDebug() << "double click chart";
    event->accept();
}

void ChartItem::wheelEvent(QGraphicsSceneWheelEvent *event) {
    int delta = event->delta();
    if (delta < 0)
        m_length -= 8.0f;
    else
        m_length += 8.0f;
    for (ChartTickItem *item : m_controlTicks) {
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
        Eigen::Vector2f nextSpacingFirstTick = nextCurve->point(1);
        double normalizedEnergy = std::clamp(nextProxy.evalYFromX(nextSpacingFirstTick.x()), 1e-5, 1.0 - 1e-5);
        nextCurve->setKeyframe(Eigen::Vector2f(nextSpacingFirstTick.x(), normalizedEnergy), 1);

        // Diffuse to next ticks
        if (!k_proxySmooth) {
            double newSpan = nextCurve->points().back().y() - normalizedEnergy;
            for (int i = 2; i < nextCurveSize - 1; ++i) {
                float tickX = nextCurve->point(i).x();
                nextCurve->setKeyframe(Eigen::Vector2f(tickX, std::min(normalizedEnergy + relativePos[i - 2] * newSpan, 1.0)), i);
            }
        } else {
            for (int i = 2; i < nextCurveSize - 1; ++i) {
                float tickX = nextCurve->point(i).x();
                nextCurve->setKeyframe(Eigen::Vector2f(tickX, std::min(nextProxy.evalYFromX(tickX), 1.0)), i);
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
        Eigen::Vector2f prevSpacingLastTick = prevCurve->point(prevCurveSize - 2);
        double normalizedEnergy = std::clamp(prevProxy.evalYFromX(prevSpacingLastTick.x()), 1e-5, 1.0 - 1e-5);
        prevCurve->setKeyframe(Eigen::Vector2f(prevSpacingLastTick.x(), normalizedEnergy), prevCurveSize - 2);

        // Diffuse to next ticks
        for (int i = 1; i < prevCurveSize - 2; ++i) {
            float tickX = prevCurve->point(i).x();
            prevCurve->setKeyframe(Eigen::Vector2f(tickX, std::min(prevProxy.evalYFromX(tickX), 1.0)), i);
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
    // qDebug() << "stride: " << m_keyframe->parentLayer()->stride(m_keyframe->parentLayer()->getVectorKeyFramePosition(m_keyframe));
    // qDebug() << "frame: " << m_keyframe->parentLayer()->getVectorKeyFramePosition(m_keyframe);
    if (m_spacing->nbPoints() - 2 != inbetweens) {
        m_spacing->resample(inbetweens);
    }
    m_nbTicks = m_spacing->nbPoints();

    for (int i = 0; i < m_spacing->nbPoints(); ++i) {
        bool fixed = i == 0 || i == m_spacing->nbPoints() - 1;
        m_controlTicks.append(new ChartTickItem(this, CONTROL, i, m_pos.x(), m_pos.y(), m_spacing->point(i).y(), i, fixed));
        m_controlTicks.back()->setParentItem(this);
    }
}

void ChartItem::clearTicks() {
    for (ChartTickItem *item : m_controlTicks) {
        item->setParentItem(nullptr);
    }
    qDeleteAll(m_controlTicks);
    m_controlTicks.resize(0);
}
