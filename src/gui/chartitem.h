/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef CHARTITEM_H
#define CHARTITEM_H

#include <QGraphicsItem>
#include <QGraphicsItemGroup>
#include <QPainter>

class Editor;
class KeyframedReal;
class VectorKeyFrame;
class Group;
class ChartTickItem;
class Curve;
class Bezier2D;

class ChartItem : public QGraphicsItem {
public:
    enum ChartMode : unsigned int { KEY=0, GROUP, PARTIAL, ONLYORDERPARTIAL, PROXY };
    enum ProxyMode : unsigned int { INOROUT=0, INANDOUT };

    ChartItem(Editor *editor, VectorKeyFrame *keyframe, QPointF pos);
    ~ChartItem();

    QRectF boundingRect() const override;

    Editor *editor() { return m_editor; }
    QPointF pos() const { return m_pos; }
    float length() const { return m_length; }
    VectorKeyFrame *keyframe() const { return m_keyframe; }
    ChartTickItem *controlTickAt(int idx) const { return m_controlTicks[idx]; }
    ChartTickItem *partialTickAt(int idx) const { return m_partialTicks[idx]; }
    unsigned int nbTicks() const { return m_nbTicks; }
    unsigned int nbFixedTicks() const;
    unsigned int nbGhostTicks() const { return nbTicks() - nbFixedTicks(); };
    unsigned int nbPartialsTicks() const { return m_partialTicks.size(); }
    Curve *spacing() const { return m_spacing; }
    ChartMode chartMode() const { return m_mode; }

    void refresh(VectorKeyFrame *keyframe);
    void updateSpacing(int tickIdx, bool refreshAllTicks=false);
    void updateSpacingProxy(ProxyMode mode);
    void resetControlTicks();
    void setChartMode(ChartMode mode);

    void setPos(QPointF pos);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) Q_DECL_OVERRIDE;
    void mousePressEvent(QGraphicsSceneMouseEvent *) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *) Q_DECL_OVERRIDE;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *) Q_DECL_OVERRIDE;
    void wheelEvent(QGraphicsSceneWheelEvent *) Q_DECL_OVERRIDE;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *) Q_DECL_OVERRIDE;

    // bool sceneEventFilter(QGraphicsItem *watched, QEvent *event) Q_DECL_OVERRIDE;

private:
    void synchronizeSpacingCurve(bool withPrev, bool withNext);

    void setSpacingCurve();

    void makeTicks(unsigned int nbTicks=0);
    void clearTicks();

    Editor *m_editor;
    QPointF m_pos;                          // position of the chart
    float m_length;                         // length of the chart
    int m_nbTicks;                          //  
    ChartMode m_mode;                       // whether we are editing the global rigid transform spacing or the local lattice spacing

    VectorKeyFrame *m_keyframe;             // keyframe that contains the displayed spacing
    QList<ChartTickItem *> m_controlTicks;  // 
    QList<ChartTickItem *> m_partialTicks;  // 
    QList<ChartTickItem *> m_proxyTicks;    // 
    Curve *m_spacing;                       // animation curve of the spacing (1D monotonic cubic spline)
};

#endif