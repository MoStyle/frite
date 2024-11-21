#include "charttickitem.h"
#include "chartitem.h"
#include "animationcurve.h"
#include "toolsmanager.h"
#include "editor.h"
#include "tools/charttool.h"
#include "playbackmanager.h"

#include <QGraphicsSceneMouseEvent>
#include <QVector2D>
#include <QPointF>

int ChartTickItem::HEIGHT = 35;
int ChartTickItem::WIDTH = 6;
// QColor ChartTickItem::colors[] = {Qt::black, QColor(0, 0, 128, 50)};
static const int CONTROL_POINTS = 4; 

ChartTickItem::ChartTickItem(ChartItem *chart, TickType type, int idx, float x, float y, double xVal, unsigned int elementIdx, bool fix) : QGraphicsRectItem() {
    m_chart = chart;
    m_type = type;
    m_idx = idx;
    m_elementIdx = elementIdx;
    m_fix = fix;
    m_x = xVal;
    m_width = WIDTH;
    m_height = (idx == 0 || idx == (chart->nbTicks() - 1)) ? HEIGHT : HEIGHT / 1.5f;
    m_yOffset = 0;
    if (m_type == TICKORDERPARTIAL) {
        m_color = QColor(255, 95, 31);
        m_width *= 2.0;
        m_height = m_width;
    } else if (m_type == TICKDRAWINGPARTIAL) {
        m_color = QColor(255, 95, 31);
        m_width *= 2.0;
        m_height = m_width;
    } else if (m_type == TICKPROXY) {
        m_color = QColor(255, 204, 0);
    } else {
        m_color = (chart->chartMode() == ChartItem::PARTIAL && m_idx > 0 && m_idx < int(chart->nbTicks() - 1)) ? QColor(78, 78, 78, 100) : Qt::black;
    }
    setRect(x + m_x * m_chart->length(), y, m_width, m_height);
    float margin = m_width - 2;
    m_renderRect = rect() - QMarginsF(margin, margin, margin, margin);
    m_chartTool = nullptr;
    updatePos();
    setPen(QPen(m_color));
    setBrush(m_color);
}

void ChartTickItem::updatePos() {
    int x = m_chart->pos().x();
    int y = m_chart->pos().y();
    setRect(x + m_x * m_chart->length(), y + (HEIGHT - m_height) / 2.0f + m_yOffset, m_width, m_height);
    float margin = m_width - 2;
    m_renderRect = rect() - QMarginsF(margin, margin, margin, margin);
}

void ChartTickItem::move(double delta) {
    m_x += delta;
    if (m_type == CONTROL) {
        m_x = std::max(m_chart->controlTickAt(m_idx - 1)->xVal() + 1e-5, m_x);
        m_x = std::min(m_chart->controlTickAt(m_idx + 1)->xVal() - 1e-5, m_x);
    }
    m_x = std::clamp(m_x, 1e-6, 1.0);
    updatePos();
}

void ChartTickItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    setBrush(QColor(78, 78, 78));
    setPen(QPen(QColor(78, 78, 78)));
    m_chart->update();
    m_chartTool = nullptr;
    Tool *tool = m_chart->editor()->tools()->currentTool();
    if (tool != nullptr && tool->isChartTool()) {
        m_chartTool = dynamic_cast<ChartTool *>(tool);
        if (m_chartTool != nullptr) m_chartTool->tickPressed(event, this);
    }
    event->accept();
}

void ChartTickItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (m_fix) {
        event->accept();
        return; 
    }
    if (m_chartTool != nullptr) m_chartTool->tickMoved(event, this);
    event->accept();
}

void ChartTickItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    setBrush(m_color);
    setPen(QPen(m_color));
    if (m_fix) return;
    if (m_chartTool != nullptr) m_chartTool->tickReleased(event, this);
    event->accept();
}

void ChartTickItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    Tool *tool = m_chart->editor()->tools()->currentTool();
    if (tool != nullptr && tool->isChartTool()) {
        m_chartTool = dynamic_cast<ChartTool *>(tool);
        if (m_chartTool != nullptr) m_chartTool->tickDoubleClick(event, this);
        m_chartTool = nullptr;
    }
    event->accept();
}

void ChartTickItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    // qDebug() << "hover";
    // setPen(QPen(QColor(50, 50, 50)));
    // setBrush(QColor(50, 50, 50));
    // update();
    event->accept();
}

void ChartTickItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    // qDebug() << "hover leave";
    // setBrush(Qt::black);
    // setPen(QPen(Qt::black));
    // update();
    event->accept();
}

void ChartTickItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (m_type == TICKDRAWINGPARTIAL || m_type == TICKORDERPARTIAL) {
        setYOffset(0);
        for (unsigned int i = 0; i < m_chart->nbPartialsTicks(); ++i) {
            if (m_chart->partialTickAt(i)->tickType() != m_type && m_chart->partialTickAt(i)->xVal() == m_x) {
                if (m_type == TICKDRAWINGPARTIAL) {
                    setYOffset(-10);
                } else {
                    setYOffset(10);
                }
            }
        }
        updatePos();
    }

    // Increase size of current frame
    float margin = m_width - 2;
    if (m_idx > 0 && m_idx < (m_chart->nbTicks() - 1) && m_chart->editor()->playback()->currentFrame() == m_chart->keyframe()->keyframeNumber() + m_idx) {
        m_renderRect = rect() - QMarginsF(margin, 0, margin, 0);
    } else {
        m_renderRect = rect() - QMarginsF(margin, margin, margin, margin);
    }

    painter->setPen(pen());
    painter->setBrush(brush());
    if (m_type == TICKDRAWINGPARTIAL || m_type == TICKORDERPARTIAL) {
        painter->save();
        QPointF center = m_renderRect.center();
        QTransform rot = QTransform().translate(center.x() + m_renderRect.width() / 2.0, center.y()).rotate(45.0).translate(-center.x(), -center.y());
        painter->setTransform(rot);
    }
    painter->drawRect(m_renderRect);
    if (m_type == TICKDRAWINGPARTIAL || m_type == TICKORDERPARTIAL) {
        painter->restore();
    }
}

void ChartTickItem::setDichotomicRight(int n) {
    unsigned int nbTicks = m_chart->nbTicks();
    float val = m_x;
    for (int i = m_idx - 1; i > 0; --i) {
        val *= 0.5f;
        m_chart->controlTickAt(i)->setXVal(val);
    }
    m_chart->updateSpacing(1, true);
}

void ChartTickItem::setDichotomicLeft(int n) {
    unsigned int nbTicks = m_chart->nbTicks();
    float val = (1.0f - m_x);
    for (int i = m_idx + 1; i < nbTicks - 1; ++i) {
        val *= 0.5f;
        m_chart->controlTickAt(i)->setXVal(1.0f - val);
    }
    m_chart->updateSpacing(1, true);
}