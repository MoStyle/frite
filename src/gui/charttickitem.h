/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef CHARTTICKITEM_H
#define CHARTTICKITEM_H

#include <QGraphicsItem>

class ChartItem;
class SpacingTool;

enum TickType {FRAME=0, CONTROL=1};

class ChartTickItem : public QGraphicsRectItem {
public:
    ChartTickItem(ChartItem *chart, TickType type, int idx, float x, float y, float xVal, int pointIdx=-1, bool fix=false);

    void move(float delta);
    void updatePos();

    ChartItem * chart() const { return m_chart; }
    TickType tickType() const { return m_type; }
    int idx() const { return m_idx; }
    int pointIdx() const { return m_pointIdx; }
    float xVal() const { return m_x; }
    bool fixed() const { return m_fix; }

    void setIdx(int idx) { m_idx = idx; }
    void setPointIdx(int pointIdx) { m_pointIdx = pointIdx; }
    void setXVal(float x) { m_x = x; updatePos(); }
    void setDichotomicRight(int n=-1);
    void setDichotomicLeft(int n=-1);

    static int HEIGHT;
    static int WIDTH;
protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *) Q_DECL_OVERRIDE;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *) Q_DECL_OVERRIDE;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *) Q_DECL_OVERRIDE;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *) Q_DECL_OVERRIDE;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) Q_DECL_OVERRIDE;

private:
    ChartItem *m_chart;
    int m_idx, m_pointIdx;
    float m_x;
    TickType m_type;
    bool m_fix;
    float m_width, m_height;
    QRectF m_renderRect;
    SpacingTool *m_spacingTool;

    QColor m_color;
    static QColor colors[];
};

#endif