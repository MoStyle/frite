#include "picktool.h"
#include "group.h"
#include "editor.h"
#include "canvascommands.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "selectionmanager.h"
#include "tabletcanvas.h"
#include "fixedscenemanager.h"
#include "toolsmanager.h"
#include "dialsandknobs.h"
#include "registrationmanager.h"

#include <QKeyEvent>

static const QStringList SHAPES({"Pick", "Lasso", "Rectangle"});
static dkStringList k_selectionShape("Select->Selection shape", SHAPES);
static const QStringList MODES({"Start keyframe", "End keyframe"});
static const QStringList MODES_TARGET({"Group", "Stroke", "Stroke segment"});
static dkStringList k_selectionMode("Select->Selection filter", MODES);
static dkStringList k_selectionModeTarget("Select->Selection filter (for registration target)", MODES_TARGET);
static dkBool k_selectTargetInCurrentKF("Select->Select target in current KF", false);

PickTool::PickTool(QObject *parent, Editor *editor) : Tool(parent, editor) {
    m_toolTips = QString("Left-click to select groups | Shift+Left-click to add groups to the current selection | Control-click (lasso) to select registration from the onion skin");
    k_selectionShape.setIndex(1);
    k_selectionModeTarget.setIndex(1);
    m_pressed = false;
    m_selectInTarget = false;
    connect(&k_selectionMode, SIGNAL(indexChanged(int)), this, SLOT(setDrawEndKeyframe(int)));
}

PickTool::~PickTool() {

}

Tool::ToolType PickTool::toolType() const {
    return Tool::Select;
}

QCursor PickTool::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}

void PickTool::toggled(bool on) {
    Tool::toggled(on);
    if (!on) {
        m_editor->tabletCanvas()->setDrawGroupColor(false);
        m_editor->tabletCanvas()->setDrawPreGroupGhosts(false);
    } else {
        m_editor->tabletCanvas()->setDrawPreGroupGhosts(k_selectionMode.index() == 1);
    }
}

void PickTool::pressed(const EventInfo& info) {
    if (!(info.mouseButton & Qt::LeftButton)){
        m_pressed = false;
        return;
    }

    m_selectInTarget = ((info.modifiers & Qt::ControlModifier));
    m_pressed = true;

    int layerNumber = m_editor->layers()->currentLayerIndex();
    int currentFrame = m_editor->playback()->currentFrame();
    Layer *layer = m_editor->layers()->currentLayer();
    VectorKeyFrame *key = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
    GroupType type = k_selectionMode.index() == 0 ? POST : PRE;


    switch (k_selectionShape.index()) {
        case 0: // "PICK"
            {
                if (!m_selectInTarget) {
                    int selectedGroup = m_editor->selection()->selectGroups(key, info.alpha, info.inbetween, type, info.pos, true);
                    m_editor->undoStack()->push(new SetSelectedGroupCommand(m_editor, layerNumber, currentFrame, selectedGroup, type, info.modifiers & Qt::ShiftModifier));
                    emit newSelectedGroup(key->selectedGroup(type));
                }   
            }
            break;

        case 1: // "LASSO"
            m_lasso = QPolygonF();
            m_lasso << info.pos;
            break;

        case 2: // "RECT"
            break;

        default:
            break;
    }

}

void PickTool::moved(const EventInfo& info) {
    if(!m_pressed) return;
    switch (k_selectionShape.index()) {
        case 0: // "PICK"
            break;

        case 1: // "LASSO"
            m_lasso << info.pos;
            break;

        case 2: // "RECT"
            break;

        default:
            break;
    }
}

void PickTool::released(const EventInfo& info) {
    if(!m_pressed) return;
    m_pressed = false;
    int layerNumber = m_editor->layers()->currentLayerIndex();
    int currentFrame = m_editor->playback()->currentFrame();
    Layer *layer = m_editor->layers()->currentLayer();
    VectorKeyFrame *key = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
    GroupType type = k_selectionMode.index() == 0 ? POST : PRE;



    if (!m_selectInTarget) {
        switch (k_selectionShape.index()) {
            case 0: // "PICK"
                break;

            case 1: // "LASSO"
                {
                    // Select post groups with at least one stroke in the lasso selection
                    std::vector<int> selectedGroups;

                    auto checkGroup = [&](Group *group) {
                        const Inbetween &inb = key->inbetween(info.inbetween);
                        for (auto intervals = group->strokes().constBegin(); intervals != group->strokes().constEnd(); ++intervals) {
                            Stroke *stroke = inb.strokes.value(intervals.key()).get();
                            for (const Interval &interval : intervals.value()) {
                                for (int i = interval.from(); i <= interval.to(); ++i) {
                                    if (m_lasso.containsPoint(QPointF(stroke->points()[i]->pos().x(),stroke->points()[i]->pos().y()), Qt::OddEvenFill)) {
                                        return true;
                                    }
                                }
                            }
                        }
                        return false;
                    };

                    for (Group *group : key->postGroups()) {
                        if (checkGroup(group)) {
                            selectedGroups.push_back(group->id());
                        }
                    }

                    if (selectedGroups.empty()) {
                        m_editor->undoStack()->beginMacro("Select Group");
                        m_editor->undoStack()->push(new SetSelectedGroupCommand(m_editor, layerNumber, currentFrame, Group::ERROR_ID, type, info.modifiers & Qt::ShiftModifier));
                        m_editor->undoStack()->endMacro();
                    } else {
                        m_editor->undoStack()->push(new SetSelectedGroupCommand(m_editor, layerNumber, currentFrame, selectedGroups, type, info.modifiers & Qt::ShiftModifier));
                    }
                    emit newSelectedGroup(key->selectedGroup(type));
                    m_lasso = QPolygonF();
                }
                break;

            case 2: // "RECT"
                break;

            default:
                break;
        }
    }
    else {  // select in target
        VectorKeyFrame *key;
        Layer * layer = m_editor->layers()->currentLayer();

        if (k_selectTargetInCurrentKF) {
            key = info.key;
        } else {
            key = info.key->nextKeyframe(); // TODO check if next keyframe exists 
            int currentFrame = layer->getVectorKeyFramePosition(info.key);
            if (layer->isVectorKeyFrameSelected(info.key) && layer->getLastKeyFrameSelected() == currentFrame){
                int frame = layer->getFirstKeyFrameSelected();
                key = layer->getVectorKeyFrameAtFrame(frame);
            }
        }

        if (key == nullptr) 
            return;

        auto strokePredicate = [&key](const Stroke *stroke) {
            return !key->preGroups().containsStroke(stroke->id());
        };

        StrokeIntervals selection;

        if (k_selectionModeTarget.index() == 0) {
            std::vector<int> selectedGroups;
            auto checkGroup = [&](Group *group) {
                for (auto intervals = group->strokes().constBegin(); intervals != group->strokes().constEnd(); ++intervals) {
                    Stroke *stroke = key->stroke(intervals.key());
                    for (const Interval &interval : intervals.value()) {
                        for (int i = interval.from(); i <= interval.to(); ++i) {
                            if (m_lasso.containsPoint(QPointF(stroke->points()[i]->pos().x(),stroke->points()[i]->pos().y()), Qt::OddEvenFill)) {
                                return true;
                            }
                        }
                    }
                }
                return false;
            };
            for (Group *group : key->postGroups()) {
                if (checkGroup(group)) {
                    for (auto it = group->strokes().constBegin(); it != group->strokes().constEnd(); ++it) {
                        selection.insert(it.key(), it.value());
                    }
                }
            }
        } else if (k_selectionModeTarget.index() == 1) {
            // select all strokes intersecting the lasso
            m_editor->selection()->selectStrokes(key, 0, [&](const StrokePtr &stroke) {
                if (key->preGroups().containsStroke(stroke->id())) return false;
                for (int i = 0; i < stroke->size(); ++i) {
                    if (m_lasso.containsPoint(QPointF(stroke->points()[i]->pos().x(),stroke->points()[i]->pos().y()), Qt::OddEvenFill)) {
                        return true;
                    }
                }
                return false;
            }, selection);
        } else {
            // select all stroke *segments* intersecting the lasso
            m_editor->selection()->selectStrokeSegments(key, m_lasso, strokePredicate, [](Point *point) { return true; }, selection);
        }


        qDebug() << "selection.size(): " << selection.size();
        if (!selection.isEmpty()) {
            m_editor->registration()->setRegistrationTarget(key, selection);
        } else {
            m_editor->registration()->clearRegistrationTarget();
        }
        m_lasso = QPolygonF();
    }

    // In case of quick select
    if (m_editor->tools()->currentTool()->isChartTool()) {
        m_editor->fixedScene()->updateChartMode(ChartItem::PARTIAL);
    }
    m_editor->fixedScene()->updateKeyChart(info.key);
    m_selectInTarget = false;
}

void PickTool::keyPressed(QKeyEvent *event) {
    if (event->key() == Qt::Key_Alt) {
        m_editor->tabletCanvas()->setDrawGroupColor(true);
    }
} 

void PickTool::keyReleased(QKeyEvent *event) {
    if (event->key() == Qt::Key_Alt) {
        m_editor->tabletCanvas()->setDrawGroupColor(false);
    }
}

void PickTool::drawUI(QPainter &painter, VectorKeyFrame *key) {
    static QPen lassoPen(QBrush(QColor(Qt::black), Qt::NoBrush), 1, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
    lassoPen.setColor(QColor(Qt::black));
    lassoPen.setCosmetic(true);
    painter.setPen(lassoPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPolygon(m_lasso);
}

void PickTool::setDrawEndKeyframe(int index) {
    if (index == 1) m_editor->tabletCanvas()->setDrawPreGroupGhosts(true);
    else m_editor->tabletCanvas()->setDrawPreGroupGhosts(false);
    m_editor->tabletCanvas()->update();
}