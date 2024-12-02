#include "pivotcreationtool.h"
#include "editor.h"
#include "tabletcanvas.h"
#include "canvascommands.h"
#include "playbackmanager.h"
#include "layermanager.h"


PivotCreationTool::PivotCreationTool(QObject *parent, Editor *editor) :
    PivotToolAbstract(parent, editor)
    , m_currentState(DEFAULT)
    , m_initialDir(Point::VectorType::Zero())
    , m_angle(0.0)
    , m_translationDone (false)
    , m_rotationDone (false)
    , m_pressed(false)
    { }


PivotCreationTool::~PivotCreationTool() {

}

Tool::ToolType PivotCreationTool::toolType() const {
    return Tool::PivotCreation;
}

QCursor PivotCreationTool::makeCursor(float scaling) const {
    return QCursor(Qt::ArrowCursor);
}

void PivotCreationTool::toggled(bool on) {
    update();
}

void PivotCreationTool::pressed(const EventInfo& info) {
    if (m_pressed) return;

    Layer * layer = m_editor->layers()->currentLayer();
    int frame = m_editor->playback()->currentFrame();

    update();

    if (!m_keyFrameSelected.contains(layer->getVectorKeyFrameAtFrame(frame)))
        return;

    if (info.mouseButton & Qt::RightButton && info.modifiers & Qt::ControlModifier){
        m_pressed = true;
        m_currentState = CONTEXT_MENU;
    }

    else if (m_translationDone && !m_rotationDone && !layer->getVectorKeyFrameAtFrame(frame)->isRotationExtracted()){
        if (info.mouseButton & Qt::RightButton){
            m_pressed = true;
            m_currentState = PIVOT_TRANSLATION;
            m_currentPos = Point::VectorType(info.pos.x(), info.pos.y());
            m_editor->undoStack()->beginMacro("Move pivot");
        }
        else if (info.mouseButton & Qt::LeftButton){
            m_pressed = true;
            m_currentState = EDIT_ROTATION;

            int frame = m_editor->playback()->currentFrame();
            Point::VectorType centerPos = layer->getPivotPosition(frame);
            m_initialDir = (Point::VectorType(info.pos.x(), info.pos.y()) - centerPos).normalized();

            VectorKeyFrame * key = layer->getVectorKeyFrameAtFrame(frame);
            int angleIdx = m_keyFrameSelected.indexOf(key);
            m_angle = m_selectedAngles[angleIdx];
        }
    }
}

void PivotCreationTool::moved(const EventInfo& info) {
    if (!m_pressed) return;

    if (m_currentState == EDIT_ROTATION){
        int frame = m_editor->playback()->currentFrame();
        Layer * layer = m_editor->layers()->currentLayer();

        Point::VectorType centerPos = layer->getPivotPosition(frame);
        Point::VectorType currentDir = (Point::VectorType(info.pos.x(), info.pos.y()) - centerPos).normalized();
        Point::Scalar atan = atan2(m_initialDir.x() * currentDir.y() - m_initialDir.y() * currentDir.x(), m_initialDir.dot(currentDir));
        m_angle += atan;
        m_initialDir = currentDir;
    }

    if (m_currentState == PIVOT_TRANSLATION){
        int frame = m_editor->playback()->currentFrame();
        int layerIdx = m_editor->layers()->currentLayerIndex();
        Point::VectorType translation(Point::VectorType(info.pos.x(), info.pos.y()) - m_currentPos);
        m_editor->undoStack()->push(new MovePivotCommand(m_editor, layerIdx, frame, translation));
        m_currentPos += translation;
    }
}

void PivotCreationTool::released(const EventInfo& info){
    if (!m_pressed) return;
    m_pressed = false;

    if (m_currentState == EDIT_ROTATION){
        int frame = m_editor->playback()->currentFrame();
        VectorKeyFrame * key = m_editor->layers()->currentLayer()->getVectorKeyFrameAtFrame(frame);
        int angleIdx = m_keyFrameSelected.indexOf(key);
        m_selectedAngles[angleIdx] = m_angle;
    }

    if (m_currentState == PIVOT_TRANSLATION){
        m_editor->undoStack()->endMacro();
    }

    if (m_currentState == CONTEXT_MENU){
        QMenu contextMenu;
        int frame = m_editor->playback()->currentFrame();
        int layerIdx = m_editor->layers()->currentLayerIndex();
        Layer * layer = m_editor->layers()->currentLayer();

        if (!m_translationDone){
            contextMenu.addAction(tr("Extract pivot"), this, [&](){
                QVector<VectorKeyFrame *> keys;
                for (VectorKeyFrame * key : m_keyFrameSelected){
                    if (!key->isTranslationExtracted())
                        keys.append(key);
                }
                m_editor->undoStack()->push(new PivotTranslationExtractionCommand(m_editor, layerIdx, keys));
                m_translationDone = true;
            });
        }

        contextMenu.addSeparator();

        if (m_translationDone && !m_rotationDone){
            contextMenu.addAction(tr("Extract rotation"), this, [&](){
                m_editor->undoStack()->beginMacro("Pivot Rotation extraction");
                QVector<VectorKeyFrame *> keys;
                QVector<float> angles;
                for (int i = 0; i < m_keyFrameSelected.size(); ++i){
                    VectorKeyFrame * key = m_keyFrameSelected[i];
                    float angle = m_selectedAngles[i];
                    if (!key->isRotationExtracted()){
                        keys.append(key);
                        angles.append(angle);
                    }
                    else if (key->isRotationExtracted() && !keys.empty()){
                        angles.append(angle);
                        m_editor->undoStack()->push(new PivotRotationExtractionCommand(m_editor, layerIdx, keys, angles));
                        keys.clear();
                        angles.clear();
                    }
                }
                if (!keys.empty()){
                    angles.append(m_selectedAngles.back());
                    m_editor->undoStack()->push(new PivotRotationExtractionCommand(m_editor, layerIdx, keys, angles));
                }
                m_editor->undoStack()->endMacro();
                m_rotationDone = true;
            });
        }

        contextMenu.exec(QCursor::pos());
    }
}

void PivotCreationTool::drawUI(QPainter &painter, VectorKeyFrame *key){
    update();
    Layer * layer = m_editor->layers()->currentLayer();
    int currentFrame = m_editor->playback()->currentFrame();
    if (m_translationDone){
        int cpt = 0;
        bool currentFrameDrawn = false;
        for (VectorKeyFrame * key : m_keyFrameSelected){
            int frame = layer->getVectorKeyFramePosition(key);

            Point::VectorType center = layer->getPivotPosition(frame);
            float angle = m_selectedAngles[cpt];
            float saturation = 0.5;
            if (currentFrame == frame && m_currentState == EDIT_ROTATION && m_pressed)
                angle = m_angle;
            if (currentFrame == frame){
                currentFrameDrawn = true;
                saturation = 1.;
            }
            drawPivot(painter, center, angle, saturation);
            cpt++;
        }
        drawTrajectory(painter, m_keyFrameSelected);
        if (!currentFrameDrawn)
            drawPivot(painter, currentFrame);
    }
}

void PivotCreationTool::update(){
    Layer * layer = m_editor->layers()->currentLayer();
    int frame = m_editor->playback()->currentFrame();

    QVector<VectorKeyFrame *> newSelectedFrames = layer->getSelectedKeyFrames();
    if (newSelectedFrames.back() == std::prev(layer->keysEnd(), 2).value()){
        newSelectedFrames.append(std::prev(layer->keysEnd(), 1).value());
    }
    if (newSelectedFrames != m_keyFrameSelected){
        m_keyFrameSelected = newSelectedFrames;
        layer->getMatchingRotation(m_keyFrameSelected, m_selectedAngles);
    }
    m_translationDone = layer->isSelectionTranslationExtracted();
    m_rotationDone = layer->isSelectionRotationExtracted();
}