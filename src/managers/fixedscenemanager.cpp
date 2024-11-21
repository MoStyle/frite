#include "fixedscenemanager.h"
#include "editor.h"
#include "tools/tool.h"
#include "toolsmanager.h"
#include "layermanager.h"
#include "playbackmanager.h"

FixedSceneManager::FixedSceneManager(Editor *editor) 
    : BaseManager(editor),
      m_keyChart(new ChartItem(editor, nullptr, QPointF(100, 100))),
      lastFrameChange(-1) {
}

FixedSceneManager::~FixedSceneManager() {

}

void FixedSceneManager::setScene(QGraphicsScene *scene) { 
    m_scene = scene; 
    m_scene->addItem(m_keyChart);
    connect(m_scene, &QGraphicsScene::sceneRectChanged, this, &FixedSceneManager::sceneResized);
}

/**
 * Refresh the spacing chart.
 * If keyframe is nullptr or no spacing tool is selected the chart will be hidden.
 * Otherwise the chart will initialize itself with the spacing of the first selected group of the given keyframe. 
*/
void FixedSceneManager::updateKeyChart(VectorKeyFrame *keyframe) {
    Tool *currentTool = m_editor->tools()->currentTool();
    if (currentTool != nullptr && !currentTool->isChartTool()) keyframe = nullptr;
    m_keyChart->refresh(keyframe);
    m_keyChart->update();
    m_scene->update();
}

/**
 * Called when the current frame has changed.
 * Check if the spacing chart needs to be refreshed.
*/
void FixedSceneManager::frameChanged(int frame) {
    Layer *layer = m_editor->layers()->currentLayer();
    // Current frame is not in any interval or no spacing tool is selected
    if (m_editor->playback()->currentFrame() < layer->getLastKeyFramePosition(frame)) {
        updateKeyChart(nullptr);
        return;
    }

    // Still in the same interval
    if (layer->getLastKeyFramePosition(frame) != layer->getLastKeyFramePosition(m_editor->playback()->currentFrame())) {
        return;
    }

    // Changed interval
    VectorKeyFrame *key = layer->getLastVectorKeyFrameAtFrame(frame, 0);
    int inbetweens = layer->stride(m_editor->playback()->currentFrame()) - 1; 
    if (key != m_keyChart->keyframe() || m_keyChart->spacing() == nullptr || (m_keyChart->nbTicks() - 2 != inbetweens)) {
        updateKeyChart(key);
    }
}

void FixedSceneManager::sceneResized(const QRectF& rect) {
    // TODO: weird stuff can happen if the timeline height is too big
    m_keyChart->setPos(QPointF(rect.width() / 2.0 - m_keyChart->length() / 2.0, 50));
}

void FixedSceneManager::updateChartMode(ChartItem::ChartMode mode) {
    // qDebug() << "update chart mode: " << mode;
    m_keyChart->setChartMode(mode);
    m_keyChart->update();
    m_scene->update();
}
