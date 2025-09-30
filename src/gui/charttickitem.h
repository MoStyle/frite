/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef CHARTTICKITEM_H
#define CHARTTICKITEM_H

#include <QGraphicsItem>

class ChartItem;
class ChartTool;


class ChartTickItem : public QGraphicsRectItem {
public:
    enum TickType {FRAME=0, CONTROL, TICKORDERPARTIAL, TICKDRAWINGPARTIAL, TICKPROXY};

    ChartTickItem(ChartItem *chart, TickType type, int idx, float x, float y, double xVal, unsigned int elementIdx, bool fix=false);

    void move(double delta);
    void updatePos();

    ChartItem * chart() const { return m_chart; }
    TickType tickType() const { return m_type; }
    int idx() const { return m_idx; }
    unsigned int elementIdx() const { return m_elementIdx; }
    double xVal() const { return m_x; }
    bool fixed() const { return m_fix; }

    void setElementIdx(unsigned int elementIdx) { m_elementIdx = elementIdx; }
    void setXVal(double x) { m_x = x; updatePos(); }
    void setDichotomicRight(int n=-1);
    void setDichotomicLeft(int n=-1);
    void setYOffset(int offset) { m_yOffset = offset; }

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
    int m_idx, m_yOffset;
    unsigned int m_elementIdx;
    double m_x;
    TickType m_type;
    bool m_fix;
    float m_width, m_height;
    QRectF m_renderRect;
    ChartTool *m_chartTool;

    QColor m_color;
    static QColor colors[];
};

#endif