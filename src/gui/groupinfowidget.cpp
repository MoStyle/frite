#include "groupinfowidget.h"
#include "editor.h"
#include "group.h"
#include "canvascommands.h"
#include "layermanager.h"
#include "playbackmanager.h"

GroupInfoWidget::GroupInfoWidget(Editor *editor, Group *group, QWidget *parent) : QWidget(parent), m_editor(editor), m_group(group) {
    setObjectName(tr("Group ")/* + QString::number(group->id())*/);
    setMinimumHeight(CELL_HEIGHT);
    setMinimumHeight(CELL_HEIGHT);
    setFixedHeight(CELL_HEIGHT);

    switch (group->type()) {
        case POST: case PRE:
            m_name = "Group " + QString::number(m_group->id());
            break;
        default:
            m_name = "Error";
            break;
    }
}

void GroupInfoWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    // drawCellBackground(painter, 0, 0, width(), height());
    if (m_group->getParentKeyframe()->selectedGroup(m_group->type()) == m_group) {
        painter.setPen(QGuiApplication::palette().color(QPalette::AlternateBase));
        painter.setBrush(QGuiApplication::palette().color(QPalette::AlternateBase));
        painter.drawRect(0, 0, width(), CELL_HEIGHT-1);
    }

    QString cc = (m_group->lattice() == nullptr || !m_group->lattice()->isSingleConnectedComponent()) ? " | /!\\" : "";

    painter.setPen(QGuiApplication::palette().color(QPalette::ButtonText));
    painter.drawText(QPoint(5, 16), m_name + " | " + tr("Strokes: ") + QString::number(m_group->size()) + cc);
    painter.setPen(QGuiApplication::palette().color(QPalette::Mid));
    painter.drawLine(0, 0, width(), 0);
    painter.drawLine(0, CELL_HEIGHT-1, width(), CELL_HEIGHT-1);
}

// void GroupInfoWidget::resizeEvent(QResizeEvent* event) {
//     update();
//     event->accept();
// }

void GroupInfoWidget::mousePressEvent(QMouseEvent* event) {
    int layerIdx = m_editor->layers()->currentLayerIndex();
    int frameIdx = m_editor->playback()->currentFrame();    
    if (event->button() & Qt::LeftButton) {
        m_editor->undoStack()->push(new SetSelectedGroupCommand(m_editor, layerIdx, frameIdx, m_group->id(), m_group->type()));
    } else {
        m_editor->undoStack()->push(new SetSelectedGroupCommand(m_editor, layerIdx, frameIdx, -1));
    }
}

void GroupInfoWidget::mouseMoveEvent(QMouseEvent* event) {
    
}

void GroupInfoWidget::mouseReleaseEvent(QMouseEvent* event) {
    
}

void GroupInfoWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    
}

void GroupInfoWidget::contextMenuEvent(QContextMenuEvent *event) {
    
}

void GroupInfoWidget::drawCellBackground(QPainter &painter, int x, int y, int width, int height) {
    QLinearGradient linearGrad(QPointF(0, y), QPointF(0, y + height));

    QColor base = QGuiApplication::palette().color(QPalette::Button);
    base.setAlpha(100);
    linearGrad.setColorAt(0, base);
    base.setAlpha(80);
    linearGrad.setColorAt(0.10, base);
    base.setAlpha(64);
    linearGrad.setColorAt(0.20, base);
    base.setAlpha(20);
    linearGrad.setColorAt(0.35, base);
    linearGrad.setColorAt(0.351, QColor(0, 0, 0, 32));
    linearGrad.setColorAt(0.66, QColor(245, 245, 245, 32));
    linearGrad.setColorAt(1, QColor(235, 235, 235, 128));

    painter.setBrush(linearGrad);
    painter.setPen(Qt::NoPen);
    painter.drawRect(x, y, width, height - 1);
}