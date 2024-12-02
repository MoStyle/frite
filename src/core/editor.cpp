#include "editor.h"

#include <Eigen/Geometry>
#include <QSet>
#include <QSvgGenerator>
#include <QUndoStack>
#include <QtWidgets>
#include <iostream>
#include <memory>

#include "canvascommands.h"
#include "colormanager.h"
#include "dialsandknobs.h"
#include "gridmanager.h"
#include "toolsmanager.h"
#include "fixedscenemanager.h"
#include "registrationmanager.h"
#include "keycommands.h"
#include "lattice.h"
#include "layer.h"
#include "layercommands.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "stylemanager.h"
#include "selectionmanager.h"
#include "tabletcanvas.h"
#include "timeline.h"
#include "viewmanager.h"
#include "layoutmanager.h"
#include "visibilitymanager.h"
#include "arap.h"
#include "tools/localmasktool.h"
#include "utils/stopwatch.h"

static dkBool k_autoBreak("Options->Layers->Auto-Break", true);
static dkBool k_exportGrid("Options->Export->Draw grid", false);
static dkBool k_exportHighRes("Options->Export->High res export", true);
static dkInt k_regularizationIt("Options->Grid->Manual regularization iterations", 100, 0, 1000, 1);
static dkBool k_onXs("Options->On X's", false);
static dkInt k_Xs("Options->X's", 2, 1, 5, 1);

dkSlider k_deformRange("Warp->Range of deformation", 75.0f, 1.0f, 1000.0f, 2.0f);
dkSlider k_splatSamplingRate("Options->Drawing->Splat sampling rate", 1, 1, 100, 1);
dkBool k_useJitter("Options->Drawing->Jitter->Jitter", false);
dkSlider k_jitterTranslation("Options->Drawing->Jitter->Translation", 4, 1, 20, 1);
dkFloat k_jitterRotation("Options->Drawing->Jitter->Rotation", 0.2, 0.01, M_PI, 0.01);
dkInt k_jitterDuration("Options->Drawing->Jitter->Duration", 1, 1, 10, 1);

extern dkInt k_cellSize;
extern dkBool k_exportOnlyCurSegment;
extern dkInt k_exportFrom;
extern dkInt k_registrationRegularizationIt;
extern dkBool k_drawSplat;
extern dkBool k_debug_out;
extern dkBool k_exportOnionSkinMode;
extern dkBool k_useDeformAsSource;

static VectorKeyFrame *g_clipboardVectorKeyFrame = nullptr;

Editor::Editor(QObject *parent) : QObject(parent) { }

Editor::~Editor() { }

bool Editor::init(TabletCanvas *canvas) {
    // Initialize managers
    m_colorManager = new ColorManager(this);
    m_layerManager = new LayerManager(this);
    m_playbackManager = new PlaybackManager(this);
    m_viewManager = new ViewManager(this);
    m_styleManager = new StyleManager(this);
    m_gridManager = new GridManager(this);
    m_registrationManager = new RegistrationManager(this);
    m_toolsManager = new ToolsManager(this);
    m_fixedSceneManager = new FixedSceneManager(this);
    m_selectionManager = new SelectionManager(this);
    m_layoutManager = new LayoutManager(this);
    m_visibilityManager = new VisibilityManager(this);

    m_layerManager->setEditor(this);
    m_playbackManager->setEditor(this);
    m_viewManager->setEditor(this);
    m_gridManager->setEditor(this);
    m_registrationManager->setEditor(this);
    m_toolsManager->setEditor(this);
    m_fixedSceneManager->setEditor(this);
    m_selectionManager->setEditor(this);
    m_layoutManager->setEditor(this);
    m_visibilityManager->setEditor(this);

    connect(this, SIGNAL(currentFrameChanged(int)), m_fixedSceneManager, SLOT(frameChanged(int)));
    connect(this, SIGNAL(timelineUpdate(int)), m_fixedSceneManager, SLOT(frameChanged(int)));

    m_undoStack = new QUndoStack(this);
    connect(m_undoStack, &QUndoStack::indexChanged, this, &Editor::updateTimeLine);

    setTabletCanvas(canvas);

    m_toolsManager->initTools();
    m_fixedSceneManager->setScene(m_tabletCanvas->fixedGraphicsScene());
    connect(&k_drawSplat, SIGNAL(valueChanged(bool)), this, SLOT(toggleDrawSplat(bool)));
    connect(&k_splatSamplingRate, &dkSlider::valueChanged, this, [&] { toggleDrawSplat(true); });
    connect(&k_onXs, SIGNAL(valueChanged(bool)), this, SLOT(makeInbetweensDirty(void)));

    m_clipboardKeyframe = nullptr;

    return true;
}

void Editor::setTabletCanvas(TabletCanvas *canvas) {
    m_tabletCanvas = canvas;
    connect(m_undoStack, &QUndoStack::indexChanged, m_tabletCanvas, &TabletCanvas::updateCurrentFrame);
    connect(&k_useJitter, SIGNAL(valueChanged(bool)), m_tabletCanvas, SLOT(updateCurrentFrame(void)));
    connect(&k_jitterTranslation, SIGNAL(valueChanged(int)), m_tabletCanvas, SLOT(updateCurrentFrame(void)));
    connect(&k_jitterRotation, SIGNAL(valueChanged(double)), m_tabletCanvas, SLOT(updateCurrentFrame(void)));
    connect(m_toolsManager, SIGNAL(toolChanged(Tool *)), canvas, SLOT(updateCursor(void)));
    connect(&k_deformRange, SIGNAL(valueChanged(int)), canvas, SLOT(updateCursor(void)));

    canvas->setEditor(this);
}

bool Editor::load(QDomElement &element, const QString &path) {
    if (element.tagName() != "editor") return false;

    if (element.hasAttribute("width") && element.hasAttribute("height")) {
        int width = element.attribute("width").toInt();
        int height = element.attribute("height").toInt();
        m_tabletCanvas->setCanvasRect(width, height);
    }

    m_tabletCanvas->hide();
    if (!m_layerManager->load(element, path)) return false;
    m_tabletCanvas->show();

    emit currentFrameChanged(playback()->currentFrame());
    QCoreApplication::processEvents(); // not very elegent way but we need the previous to be processed immediately 

    m_toolsManager->currentTool()->toggled(true);
    m_fixedSceneManager->updateKeyChart(m_layerManager->currentLayer()->getLastKey(m_playbackManager->currentFrame()));

    return true;
}

bool Editor::save(QDomDocument &doc, QDomElement &root, const QString &path) const {
    QDomElement element = doc.createElement("editor");
    element.setAttribute("width", m_tabletCanvas->canvasRect().width());
    element.setAttribute("height", m_tabletCanvas->canvasRect().height());
    m_layerManager->save(doc, element, path);

    root.appendChild(element);
    return true;
}

// !TODO
void Editor::cut() {
    m_undoStack->beginMacro("Cut");
    copy();
    m_undoStack->endMacro();
}

void Editor::copy() {
    Layer *layer = m_layerManager->currentLayer();
    VectorKeyFrame *keyframe = prevKeyFrame();
    m_clipboardKeyframe = keyframe;
    m_clipboardStrokes.clear();

    for (Group *group : keyframe->selection().selectedPostGroups()) {
        m_clipboardStrokes.push_back(group->strokes());
    }
}

void Editor::paste() {
    Layer *layer = m_layerManager->currentLayer();
    VectorKeyFrame *keyframe = prevKeyFrame();
    int layerIdx = m_layerManager->currentLayerIndex();
    int frame = m_playbackManager->currentFrame();

    if (m_clipboardKeyframe == nullptr || m_clipboardKeyframe == keyframe || m_clipboardStrokes.empty()) return;

    m_undoStack->beginMacro("Paste groups");
    for (const StrokeIntervals &strokeIntervals : m_clipboardStrokes) {
        m_undoStack->push(new AddGroupCommand(this, layerIdx, frame));
        Group *newGroup = keyframe->postGroups().lastGroup();
        for (auto it = strokeIntervals.constBegin(); it != strokeIntervals.constEnd(); ++it) {
            Stroke *clipboardStroke = m_clipboardKeyframe->stroke(it.key());
            for (const Interval &interval : it.value()) {
                StrokePtr newStroke = std::make_shared<Stroke>(*clipboardStroke, keyframe->pullMaxStrokeIdx(), interval.from(), interval.to());
                m_undoStack->push(new DrawCommand(this, layerIdx, frame, newStroke, newGroup->id(), false));
            }
        }
    }
    m_undoStack->endMacro();
}

void Editor::increaseCurrentKeyExposure() {
    m_undoStack->push(new ChangeExposureCommand(this, m_layerManager->currentLayerIndex(), m_playbackManager->currentFrame(), 1));
}

void Editor::decreaseCurrentKeyExposure() {
    Layer *layer = m_layerManager->currentLayer();
    if (layer->stride(layer->getLastKeyFramePosition(m_playbackManager->currentFrame())) <= 1) return;
    m_undoStack->push(new ChangeExposureCommand(this, m_layerManager->currentLayerIndex(), m_playbackManager->currentFrame(), -1));
}

/**
 * Deselect all groups and trajectories in the current keyframe  
 */
void Editor::deselectAll() { 
    VectorKeyFrame *key = prevKeyFrame();
    int layer = m_layerManager->currentLayerIndex();
    int currentFrame = m_playbackManager->currentFrame();
    if (key == nullptr || m_toolsManager->currentTool()->needEscapeFocus()) return;
    m_undoStack->beginMacro("Deselect All");
    m_undoStack->push(new SetSelectedGroupCommand(this, layer, currentFrame, Group::ERROR_ID));
    m_undoStack->push(new SetSelectedTrajectoryCommand(this, layer, currentFrame, nullptr));
    m_undoStack->endMacro();
}

/**
 * Return the current time step (in [0,1]) between the last and next keyframe.  
 * If the current frame is keyframe, return 0
 * If the current frame is after the last keyframe of the layer return 1
 * Otherwise the value is linearly interpolated between 0 and 1
 */
qreal Editor::alpha(int frame, Layer *layer) {
    if (layer == nullptr) layer = m_layerManager->currentLayer();
    if (layer) {
        if (frame >= layer->getMaxKeyFramePosition()) return 1.0;
        if (k_onXs) frame -= k_Xs - 1 - (frame % (k_Xs));
        int prevKey = layer->getLastKeyFramePosition(frame);
        int nextKey = layer->getNextKeyFramePosition(frame);
        if (nextKey == prevKey + 1) return 0.0;
        return qreal(frame - prevKey) / (nextKey - prevKey);
    }
    return 0.0;
}

/**
 * Return the alpha value of the current frame in the timeline
 */
qreal Editor::currentAlpha() {
    return alpha(m_playbackManager->currentFrame());
}

/**
 * Change the current frame 
 */
void Editor::scrubTo(int frame) {
    if (frame < 1) {
        frame = 1;
    }
    m_playbackManager->setCurrentFrame(frame);
    emit currentFrameChanged(frame);
    emit alphaChanged(alpha(frame));
}

void Editor::scrubForward() { scrubTo(m_playbackManager->currentFrame() + 1); }

void Editor::scrubBackward() {
    if (m_playbackManager->currentFrame() > 1) {
        scrubTo(m_playbackManager->currentFrame() - 1);
    }
}

void Editor::addKey() { m_undoStack->push(new AddKeyCommand(this, layers()->currentLayerIndex(), m_playbackManager->currentFrame())); }

int Editor::addKeyFrame(int layerNumber, int frameIndex, bool updateCurves) {
    Layer *layer = m_layerManager->layerAt(layerNumber);
    if (layer == nullptr) return -1;
    VectorKeyFrame *prev = layer->getLastKey(frameIndex);
    layer->addNewEmptyKeyAt(frameIndex);
    if (prev && updateCurves) prev->updateCurves();
    emit currentFrameChanged(frameIndex);
    // scrubTo(frameIndex);
    return frameIndex;
}

void Editor::removeKey() {
    int currentFrame = m_playbackManager->currentFrame();
    if (layers()->currentLayer()->keyExists(currentFrame) && layers()->currentLayer()->getMaxKeyFramePosition() > currentFrame) {
        // remove one key frame
        if (layers()->currentLayer()->nbKeys() == 2) {
            m_undoStack->push(new ClearCommand(this, layers()->currentLayerIndex(), currentFrame));
        } else {
            m_undoStack->push(new RemoveKeyCommand(this, layers()->currentLayerIndex(), currentFrame));
        }
    } else {
        // remove one image
        m_undoStack->push(new ChangeExposureCommand(this, layers()->currentLayerIndex(), currentFrame, -1));
    }
}

void Editor::removeKeyFrame(int layerNumber, int frameIndex) {
    Layer *layer = m_layerManager->layerAt(layerNumber);
    if (layer != nullptr) {
        if (layer->keyExists(frameIndex)) {
            layer->deselectAllKeys();
            layer->removeKeyFrame(frameIndex);
            VectorKeyFrame *prev = layer->getLastKey(frameIndex);
            if (prev) prev->updateCurves();
            emit currentFrameChanged(frameIndex);
            // scrubBackward();
            m_tabletCanvas->update();
        }
    }
    emit layers()->currentLayerChanged(layerNumber);  // trigger timeline repaint.
}

/**
 * Update the specified inbetween frame of the given keyframe.
 * If the stride has changed, all inbetweens between the keyframe and the next one are reset.
 * @param keyframe The keyframe storing the inbetweens
 * @param inbetween The relative index of the inbetween to update (0 <= inbetween <= stride)
 * @param stride The number of frames between the keyframe and the next one
*/
int Editor::updateInbetweens(VectorKeyFrame *keyframe, int inbetween, int stride) {
    if (inbetween > stride) inbetween = stride;
    if (keyframe->inbetweens().empty() || stride != keyframe->inbetweens().size() - 1) {
        // qDebug() << "Inbetweens size: " << keyframe->inbetweens().size() << "  != stride: " << stride;
        keyframe->clearInbetweens();
        keyframe->initInbetweens(stride);
    }
    if (stride == 0 || inbetween < 0) return inbetween;
    if (!m_exporting && QOpenGLContext::currentContext() != m_tabletCanvas->context()) m_tabletCanvas->makeCurrent();
    keyframe->bakeInbetween(this, keyframe->parentLayer()->getVectorKeyFramePosition(keyframe), inbetween, stride);
    return inbetween;
}

void Editor::deleteAllEmptyGroups(int layerNumber, int frameIndex) {
    VectorKeyFrame *key = m_layerManager->layerAt(layerNumber)->getLastVectorKeyFrameAtFrame(frameIndex, 0);
    std::vector<int> postGroupsToRemove, preGroupsToRemove;
    for (Group *group : key->postGroups()) {
        if (group->id() != Group::MAIN_GROUP_ID && group->size() == 0) {
            postGroupsToRemove.push_back(group->id());
        }
    }
    for (Group *group : key->preGroups()) {
        if (group->size() == 0) {
            preGroupsToRemove.push_back(group->id());
        }
    }
    m_undoStack->beginMacro("Delete empty groups");
    for (int id : postGroupsToRemove) {
        m_undoStack->push(new RemoveGroupCommand(this, layerNumber, frameIndex, id, POST));
    }
    for (int id : preGroupsToRemove) {
        m_undoStack->push(new RemoveGroupCommand(this, layerNumber, frameIndex, id, PRE));
    }
    m_undoStack->endMacro();
}

void Editor::exportFrames(const QString &path, QSize exportSize, bool transparency) {
    QFileInfo info(path);
    int maxFrame = m_layerManager->maxFrame();
    int nbDigits = QString::number(maxFrame).length();

    srand(0);
    for (int i = 0; i < m_layerManager->layersCount(); i++) {
        Layer *layer = m_layerManager->layerAt(i);
        if (layer->color != Qt::black) continue;
        QColor newColor = QColor(rand() % 255, rand() % 255, rand() % 255);
        while (newColor.red() + newColor.green() + newColor.blue() > 150 * 3) newColor = QColor(rand() % 255, rand() % 255, rand() % 255);
        layer->color = newColor;
    }

    if (k_exportOnionSkinMode) maxFrame = k_exportFrom;
    if (!k_exportHighRes) exportSize = QSize(m_tabletCanvas->canvasRect().width(), m_tabletCanvas->canvasRect().height());
    
    // Destroy buffers made with the OpenGLCanvas default FBO
    m_layerManager->destroyBuffers();

    if (QOpenGLContext::currentContext() != m_tabletCanvas->context()) m_tabletCanvas->makeCurrent();
    m_exporting = true;
    for (int frame = k_exportFrom; frame <= maxFrame; frame++) {
        QString frameNumberString = QString::number(frame);
        while (frameNumberString.length() < nbDigits) frameNumberString.prepend("0");


        double scaleW = exportSize.width() / m_tabletCanvas->canvasRect().width();
        double scaleH = exportSize.height() / m_tabletCanvas->canvasRect().height();
        if (info.completeSuffix() == "svg") {
            scrubTo(frame);
            QRectF targetRect(QPointF(0, 0), exportSize);

            QSvgGenerator generator;
            generator.setFileName(info.absolutePath() + "/" + info.baseName() + "_" + frameNumberString + "." + info.completeSuffix());
            generator.setSize(exportSize);
            generator.setViewBox(targetRect.toRect());
            QPainter painter;
            painter.begin(&generator);            
            painter.save();
            m_tabletCanvas->initializeFBO(exportSize.width(), exportSize.height());
            painter.scale(scaleW, scaleH);
            painter.translate( m_tabletCanvas->canvasRect().width()/2,  m_tabletCanvas->canvasRect().height()/2);
            m_tabletCanvas->paintGLInit(exportSize.width(), exportSize.height(), true, true);
            m_tabletCanvas->drawCanvas(true);
            m_tabletCanvas->paintGLRelease(true);
            painter.restore();

            QImage fboImage = m_tabletCanvas->grabCanvasFramebuffer();
            QImage image(fboImage.constBits(), fboImage.width(), fboImage.height(), QImage::Format_ARGB32);
            painter.drawImage(QPoint(0, 0), image); // the image is on top of vector drawings :(

            painter.save();
            painter.scale(scaleW, scaleH);
            painter.translate(m_tabletCanvas->canvasRect().width()/2,  m_tabletCanvas->canvasRect().height()/2);
            m_tabletCanvas->drawToolGizmos(painter);
            if (k_exportGrid) {
                Layer *layer = m_layerManager->currentLayer();
                VectorKeyFrame *keyframe = layer->getLastKey(frame);
                int inbetween = layer->inbetweenPosition(frame);
                int stride = layer->stride(frame);
                qreal alphaLinear = alpha(frame);
                for (Group *group : keyframe->postGroups()) {
                    group->setShowGrid(true);
                    if (group->lattice()->isArapPrecomputeDirty()) group->lattice()->precompute();
                    group->lattice()->interpolateARAP(alphaLinear, group->spacingAlpha(alphaLinear), keyframe->rigidTransform(alphaLinear), false);
                    if (inbetween == 0) group->drawGrid(painter, 0, REF_POS);
                    else                group->drawGrid(painter, inbetween, INTERP_POS);
                    group->setShowGrid(false);
                }
            }
            painter.restore();
            
            painter.save();
            m_tabletCanvas->fixedGraphicsScene()->render(&painter, targetRect);
            painter.restore();
            painter.end();
        } else {
            QColor bgColor(Qt::white);
            QRectF targetRect(QPointF(0, 0), exportSize);

            scrubTo(frame);

            QImage img(exportSize, QImage::Format_ARGB32_Premultiplied);
            img.fill(bgColor);
            QPainter painter(&img);
            painter.setWorldMatrixEnabled(true);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

            painter.save();
            m_tabletCanvas->initializeFBO(exportSize.width(), exportSize.height());
            painter.scale(scaleW, scaleH);
            painter.translate( m_tabletCanvas->canvasRect().width() / 2,  m_tabletCanvas->canvasRect().height() / 2);
            painter.beginNativePainting();
            m_tabletCanvas->paintGLInit(exportSize.width(), exportSize.height(), true, true);
            m_tabletCanvas->drawCanvas(true);
            m_tabletCanvas->paintGLRelease(true);
            painter.endNativePainting();
            painter.restore();

            m_tabletCanvas->resolveMSFramebuffer();

            QImage fboImage = m_tabletCanvas->grabCanvasFramebuffer();
            QImage image(fboImage.constBits(), fboImage.width(), fboImage.height(), QImage::Format_ARGB32);
            painter.drawImage(QPoint(0, 0), image); // the image is on top of vector drawings :(

            painter.save();
            painter.scale(scaleW, scaleH);
            painter.translate(m_tabletCanvas->canvasRect().width()/2, m_tabletCanvas->canvasRect().height()/2);
            m_tabletCanvas->drawToolGizmos(painter);
            if (k_exportGrid) {
                Layer *layer = m_layerManager->currentLayer();
                VectorKeyFrame *keyframe = layer->getLastKey(frame);
                int inbetween = layer->inbetweenPosition(frame);
                int stride = layer->stride(frame);
                qreal alphaLinear = alpha(frame);

                if (inbetween == 0 && frame == 9) {
                    VectorKeyFrame *previousKeyframe = layer->getVectorKeyFrameAtFrame(layer->getPreviousKeyFramePosition(frame));
                    for (Group *group : previousKeyframe->postGroups()) {
                        group->setShowGrid(true);
                        group->drawGrid(painter, stride, TARGET_POS);
                        group->setShowGrid(false);
                    }
                }

                for (Group *group : keyframe->postGroups()) {
                    group->setShowGrid(true);
                    if (group->lattice()->isArapPrecomputeDirty()) group->lattice()->precompute();
                    group->lattice()->interpolateARAP(alphaLinear, group->spacingAlpha(alphaLinear), keyframe->rigidTransform(alphaLinear), false);
                    if (inbetween == 0) group->drawGrid(painter, 0, REF_POS);
                    else                group->drawGrid(painter, inbetween, INTERP_POS);
                    group->setShowGrid(false);
                }
            }
            painter.restore();

            painter.save();
            painter.translate(m_tabletCanvas->canvasRect().width()/2, m_tabletCanvas->canvasRect().height()/2);
            m_tabletCanvas->fixedGraphicsScene()->render(&painter, targetRect);
            painter.restore();
            painter.end();

            img.save(info.absolutePath() + "/" + info.baseName() + "_" + frameNumberString + "." + info.completeSuffix());
        }
        std::cout << "Frame " << frame << " has been exported" << std::endl;
    }
    m_exporting = false;
    m_tabletCanvas->initializeFBO(m_viewManager->canvasSize().width(), m_viewManager->canvasSize().height());
    m_tabletCanvas->doneCurrent();
}

VectorKeyFrame *Editor::currentKeyFrame() {
    Layer *layer = m_layerManager->currentLayer();
    return layer->getVectorKeyFrameAtFrame(m_playbackManager->currentFrame());
}

VectorKeyFrame *Editor::prevKeyFrame() {
    Layer *layer = m_layerManager->currentLayer();
    return layer->getLastVectorKeyFrameAtFrame(m_playbackManager->currentFrame(), 0);
}

void Editor::registerFromRestPosition(VectorKeyFrame * key, bool registerToNextKeyframe){
    if (key == nullptr) return;
    
    if (registerToNextKeyframe) {
        VectorKeyFrame *target = key->nextKeyframe();

        int lastFrame = layers()->currentLayer()->getMaxKeyFramePosition();
        int currentFrame = layers()->currentLayer()->getVectorKeyFramePosition(key);
        if (layers()->currentLayer()->isVectorKeyFrameSelected(key) && layers()->currentLayer()->getLastKeyFrameSelected() == currentFrame){
            int frame = layers()->currentLayer()->getFirstKeyFrameSelected();
            target = layers()->currentLayer()->getVectorKeyFrameAtFrame(frame);
        }
        m_registrationManager->setRegistrationTarget(target);
    }
    const QMap<int, Group *> &groups = key->selection().selectedPostGroups().empty() ? key->groups(POST) : key->selection().selectedPostGroups();
    bool multipleGroupsSelected = groups.size() > 1;
    Point::Affine scalingMat;
    if (multipleGroupsSelected) {
        m_registrationManager->preRegistration(groups, TARGET_POS);
        scalingMat.setIdentity();
        scalingMat.scale(m_registrationManager->preRegistrationScaling());
    }
    for (Group *group : groups) {
        m_registrationManager->registration(group, TARGET_POS, TARGET_POS, !multipleGroupsSelected);
        if (multipleGroupsSelected) group->lattice()->setScaling(scalingMat);
    }
    if (registerToNextKeyframe) m_registrationManager->clearRegistrationTarget();
    key->makeInbetweensDirty();
    m_tabletCanvas->update();
}

void Editor::duplicateKey() {
    Layer *layer = m_layerManager->layerAt(layers()->currentLayerIndex());
    if (layer != nullptr) {
        m_undoStack->beginMacro("Clone key");
        copy();
        addKey();
        paste();
        m_undoStack->endMacro();
    }
}

void Editor::setCurrentLayer(int layerNumber) {
    layers()->setCurrentLayer(layerNumber);
    m_tabletCanvas->update();
}

void Editor::clearCurrentFrame() {
    Layer *layer = m_layerManager->currentLayer();
    if (layer) {
        if (layer->keyExists(m_playbackManager->currentFrame())) {
            m_undoStack->push(new ClearCommand(this, m_layerManager->currentLayerIndex(), m_playbackManager->currentFrame()));
        }
    }
}

/**
 * Add the given stroke to the canvas.
 * If the current frame is not a keyframe, a keyframe is added.
 * The stroke is added to the selected group, if no group is selected then the stroke is added to the default group.
*/
void Editor::addStroke(StrokePtr stroke) {
    Layer *layer = m_layerManager->currentLayer();
    if (stroke->points().size() < 2 || layer == nullptr) return;

    m_undoStack->beginMacro("Update keyframe");
    int currentFrame = m_playbackManager->currentFrame();
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
    int group = Group::MAIN_GROUP_ID;
    GroupType type = POST;
    if (!keyframe->selection().selectedPostGroups().empty()) {
        group = keyframe->selection().selectedPostGroups().constBegin().value()->id();
        type = POST;
    } else if (!keyframe->selection().selectedPreGroups().empty()) {
        group = keyframe->selection().selectedPreGroups().constBegin().value()->id();
        type = PRE;
    }

    m_undoStack->push(new DrawCommand(this, m_layerManager->currentLayerIndex(), currentFrame, stroke, group, true, type));

    m_undoStack->endMacro();
}

void Editor::addEndStroke(StrokePtr stroke) {
    Layer *layer = m_layerManager->currentLayer();
    if (stroke->points().size() < 2 || layer == nullptr) return;
    int prevkey = m_playbackManager->currentFrame();
    if (!layer->keyExists(prevkey) || prevkey == layer->getMaxKeyFramePosition()) return;
    VectorKeyFrame *keyframe = layer->getVectorKeyFrameAtFrame(prevkey);
    if (keyframe->selection().selectedPreGroups().empty()) return;
    m_undoStack->beginMacro("Update keyframe");
    int group = keyframe->selection().selectedPreGroups().constBegin().value()->id();
    m_undoStack->push(new DrawCommand(this, m_layerManager->currentLayerIndex(), prevkey, stroke, group, true, PRE));
    m_undoStack->endMacro();
}

void Editor::setBackwardColor(const QColor &backwardColor) {
    m_backwardColor = backwardColor;
    m_tabletCanvas->updateCurrentFrame();
}

void Editor::setForwardColor(const QColor &forwardColor) {
    m_forwardColor = forwardColor;
    m_tabletCanvas->updateCurrentFrame();
}

void Editor::setEqValues(const EqualizerValues &value) {
    m_eqValues = value;
    m_tabletCanvas->updateCurrentFrame();
}

void Editor::setEqMode(int value) {
    m_eqMode = EqualizedMode(value);
    m_tabletCanvas->updateCurrentFrame();
}

void Editor::setTintFactor(int value) {
    m_tintFactor = value;
    m_tabletCanvas->updateCurrentFrame();
}

void Editor::setGhostMode(bool ghostMode) {
    m_ghostMode = ghostMode;
}

void Editor::updateUI(VectorKeyFrame *key) {
    m_fixedSceneManager->updateKeyChart(key);                                           // Tell the spacing chart widget to show the selected group chart
    emit m_tabletCanvas->groupsModified(POST);                                          // Update the group list UI
    emit m_tabletCanvas->groupsModified(PRE);                                           // Update the group list UI
}

void Editor::deselectInAllLayers() {
    m_undoStack->beginMacro("Deselect All");
    for (int l = m_layerManager->layersCount() - 1; l >= 0; l--) {
        Layer *layer = m_layerManager->layerAt(l);
        for (auto it = layer->keysBegin(); it != layer->keysEnd(); ++it) {
            m_undoStack->push(new SetSelectedGroupCommand(this, l, it.key(), Group::ERROR_ID));
            m_undoStack->push(new SetSelectedTrajectoryCommand(this, l, it.key(), nullptr));
        }
    }
    m_undoStack->endMacro();
}

// TODO: set as qcommand
void Editor::clearARAPWarp() {
    Layer *layer = m_layerManager->currentLayer();
    if (layer) {
        int currentFrame = m_playbackManager->currentFrame();
        VectorKeyFrame *keyFrame = layer->getLastVectorKeyFrameAtFrame(currentFrame, 0);
        if (keyFrame == nullptr) return;

        for (Group *group : keyFrame->selection().selectedPostGroups()) {
            if (group->lattice() != nullptr) {
                group->lattice()->resetDeformation();
            }
        }

        m_registrationManager->clearRegistrationTarget();

        if (keyFrame->selectedGroup(POST) == nullptr) {
            keyFrame->resetRigidDeformation();
        }

        keyFrame->makeInbetweensDirty();
        
        scrubTo(currentFrame);
    }
}

void Editor::toggleOnionSkin() {
    m_undoStack->push(new SwitchOnionCommand(m_layerManager, m_layerManager->currentLayerIndex()));
}

void Editor::toggleHasMask() {
    m_undoStack->push(new SwitchHasMaskCommand(m_layerManager, m_layerManager->currentLayerIndex()));
}

void Editor::makeTrajectoryC1Continuous() {
    m_undoStack->push(new MakeTrajectoryC1Command(this, m_layerManager->currentLayerIndex(), m_playbackManager->currentFrame(), prevKeyFrame()->selection().selectedTrajectory()));
    m_tabletCanvas->update();
}

// TODO: convert all the functions below to qundocommands
void Editor::makeGroupFadeOut() {
    for (Group *group : prevKeyFrame()->selection().selectedPostGroups()) {
        group->setDisappear(!group->disappear());
    }
    m_tabletCanvas->update();
}

/**
 * Break the selected groups into new groups with a single connected component each.
 * If the selected group is already has only one connected component then nothing happens. 
 */
std::set<int> Editor::splitGridIntoSingleConnectedComponent() {
    Layer *layer = m_layerManager->currentLayer();
    int layerIdx = m_layerManager->currentLayerIndex();
    int currentFrame = m_playbackManager->currentFrame();
    VectorKeyFrame *key = prevKeyFrame();
    int quadKey; QuadPtr quad;
    std::vector<int> groupsToRemove;
    std::set<int> newGroups;
    const QMap<int, Group *> &groups = key->selection().selectedPostGroups().empty() ? key->postGroups() : key->selection().selectedPostGroups();

    m_undoStack->beginMacro("Break group");
    for (Group *group : groups) {
        // Get list of CC
        std::vector<std::vector<int>> connectedComponents;
        group->lattice()->getConnectedComponents(connectedComponents); // TODO: check flag

        if (connectedComponents.size() <= 1) continue;

        // Make new groups & grids
        for (const std::vector<int> &connectedComponent : connectedComponents) {
            m_undoStack->push(new AddGroupCommand(this, layerIdx, currentFrame));
            Group *newGroup = key->postGroups().lastGroup();
            newGroup->setGrid(new Lattice(*group->lattice(), connectedComponent));
            newGroups.insert(newGroup->id());

            // Get list of strokes segments in the cc
            for (auto strokeIt = group->strokes().begin(); strokeIt != group->strokes().end(); ++strokeIt) {
                Intervals newIntervals;
                for (Interval &interval : strokeIt.value()) {
                    if (newGroup->lattice()->contains(key->stroke(strokeIt.key())->points()[interval.from()]->pos(), REF_POS, quad, quadKey)) {
                        newIntervals.append(interval);
                        // TODO copy instead of recomputing
                        m_gridManager->bakeStrokeInGrid(newGroup->lattice(), key->stroke(strokeIt.key()), interval.from(), interval.to());
                        newGroup->lattice()->bakeForwardUV(key->stroke(strokeIt.key()), interval, newGroup->uvs());
                    }
                }
                if (!newIntervals.empty()) {
                    newGroup->addStroke(strokeIt.key(), newIntervals);
                }
            }
        }
        groupsToRemove.push_back(group->id());
    }

    deselectAll();
    
    for (int groupId : groupsToRemove) {
        if (groupId == Group::MAIN_GROUP_ID) {
            m_undoStack->push(new ClearMainGroupCommand(this, layerIdx, currentFrame));
        } else {
            m_undoStack->push(new RemoveGroupCommand(this, layerIdx, currentFrame, groupId, POST));
        }
    }
    m_undoStack->endMacro();

    key->makeInbetweensDirty();

    return newGroups;
}

void Editor::regularizeLattice() {
    VectorKeyFrame *key = prevKeyFrame();
    for (Group *group : key->selection().selectedPostGroups()) {
        if (group->lattice() == nullptr) continue;
        Arap::regularizeLattice(*group->lattice(), k_useDeformAsSource ? DEFORM_POS : REF_POS, TARGET_POS, k_regularizationIt, true, false);
        group->setGridDirty();
        key->makeInbetweensDirty();
    }
    m_tabletCanvas->update();
}

void Editor::registerFromRestPosition() {
    VectorKeyFrame *key = currentKeyFrame();
    registerFromRestPosition(key, m_registrationManager->registrationTargetEmpty());
}

void Editor::registerFromTargetPosition() {
    VectorKeyFrame *key = prevKeyFrame();
    bool registerToNextKeyframe = m_registrationManager->registrationTargetEmpty();
    if (registerToNextKeyframe) m_registrationManager->setRegistrationTarget(key->nextKeyframe());
    for (Group *group : key->selection().selectedPostGroups()) {
        m_registrationManager->registration(group, TARGET_POS, REF_POS, false, 1, k_registrationRegularizationIt);
    }
    if (registerToNextKeyframe) m_registrationManager->clearRegistrationTarget(); 
    key->makeInbetweensDirty();
    m_tabletCanvas->update();
}

void Editor::changeGridSize() {
    bool ok;
    int cellSize = QInputDialog::getInt(m_tabletCanvas, tr("Change grid size"), tr("Size (px)"), 1, 1, 100, 1, &ok);
    if (!ok) return;
    VectorKeyFrame *key = prevKeyFrame();
    for (Group *group : key->selection().selectedPostGroups()) {
        m_gridManager->constructGrid(group, m_viewManager, cellSize);
    }
    key->makeInbetweensDirty();
    m_tabletCanvas->update();
}

void Editor::expandGrid() {
    VectorKeyFrame *key = prevKeyFrame();
    for (Group *group : key->selection().selectedPostGroups()) {
        std::vector<int> newQuads;
        for (QuadPtr q : group->lattice()->quads()) q->setMiscFlag(false);
        m_gridManager->addOneRing(group->lattice(), newQuads);
        m_gridManager->propagateDeformToOneRing(group->lattice(), newQuads);
        group->setGridDirty();
        group->lattice()->setBackwardUVDirty(true);
    }
    key->makeInbetweensDirty();
    m_tabletCanvas->update();
}

void Editor::clearGrid() {
    VectorKeyFrame *key = prevKeyFrame();
    for (Group *group : key->selection().selectedPostGroups()) {
        m_gridManager->constructGrid(group, m_viewManager, k_cellSize);
    }
    key->makeInbetweensDirty();
    m_tabletCanvas->update();
}

void Editor::copyGroupToNextKeyFrame(bool makeBreakdown) {
    Layer *layer = m_layerManager->currentLayer();
    int currentFrame = m_playbackManager->currentFrame();
    VectorKeyFrame *key = prevKeyFrame();
    VectorKeyFrame *next = key->nextKeyframe();
    if (next == layer->getVectorKeyFrameAtFrame(layer->getMaxKeyFramePosition())) {
        m_undoStack->push(new AddKeyCommand(this, layers()->currentLayerIndex(), layer->getMaxKeyFramePosition()));
    }
    next = key->nextKeyframe();
    for (Group *group : key->selection().selectedPostGroups()) {
        key->copyDeformedGroup(next, group, makeBreakdown);
    }
    m_tabletCanvas->update();
}

void Editor::convertToBreakdown() {
    Layer *layer = m_layerManager->currentLayer();
    int layerIdx = m_layerManager->currentLayerIndex();
    int currentFrame = m_playbackManager->currentFrame();
    VectorKeyFrame *keyframe = prevKeyFrame();
    if (!layer->keyExists(currentFrame)) {
        float t = alpha(currentFrame);
        m_undoStack->push(new AddBreakdownCommand(this, layerIdx, layer->getLastKeyFramePosition(currentFrame), currentFrame, t));
        m_fixedSceneManager->updateKeyChart(keyframe);
    }
    m_tabletCanvas->update();
}

/**
 * Toggle cross-fade for all selected groups.
 * If no group is selected, toggle cross-fade for all groups.
 */
void Editor::toggleCrossFade() {
    VectorKeyFrame *key = prevKeyFrame();
    VectorKeyFrame *nextKey = key->nextKeyframe();
    Layer *layer = m_layerManager->currentLayer();
    int layerIdx = m_layerManager->currentLayerIndex();
    int currentFrame = m_playbackManager->currentFrame();
    int nextFrame = layer->getVectorKeyFramePosition(nextKey);
    const QMap<int, Group *> &groups = key->selection().selectedPostGroups().empty() ? key->postGroups() : key->selection().selectedPostGroups();
    for (Group *group : groups) {
        Group *nextPre = group->nextPreGroup();
        if (nextPre != nullptr) {
            m_undoStack->push(new RemoveCorrespondenceCommand(this, layerIdx, currentFrame, group->id()));
            // TODO remove intra corresp?
            m_undoStack->push(new RemoveGroupCommand(this, layerIdx, nextFrame, nextPre->id(), PRE));
        } else {
            key->toggleCrossFade(this, group);
        }
    }
    m_tabletCanvas->update();
}

/**
 * Clear cross-fade for all selected groups.
 * If no group is selected, Clear cross-fade for all groups.
 */
void Editor::clearCrossFade() {
    VectorKeyFrame *key = prevKeyFrame();
    VectorKeyFrame *nextKey = key->nextKeyframe();
    Layer *layer = m_layerManager->currentLayer();
    int layerIdx = m_layerManager->currentLayerIndex();
    int currentFrame = m_playbackManager->currentFrame();
    int nextFrame = layer->getVectorKeyFramePosition(nextKey);
    const QMap<int, Group *> &groups = key->selection().selectedPostGroups().empty() ? key->postGroups() : key->selection().selectedPostGroups();
    for (Group *group : groups) {
        Group *nextPre = group->nextPreGroup();
        m_undoStack->push(new RemoveCorrespondenceCommand(this, layerIdx, currentFrame, group->id()));
        if (nextPre != nullptr) {
            m_undoStack->push(new RemoveGroupCommand(this, layerIdx, nextFrame, nextPre->id(), PRE));
        }
    }
    m_tabletCanvas->update();
}

void Editor::deleteGroup() {
    int layerIdx = m_layerManager->currentLayerIndex();
    int currentFrame = m_playbackManager->currentFrame();
    VectorKeyFrame *key = prevKeyFrame();
    m_undoStack->beginMacro("Delete groups");
    for (Group *group : key->selection().selectedPostGroups()) {
        m_undoStack->push(new RemoveGroupCommand(this, layerIdx, currentFrame, group->id(), POST));
    }
    key->selection().clearSelectedPostGroups();
    for (Group *group : key->selection().selectedPreGroups()) {
        m_undoStack->push(new RemoveGroupCommand(this, layerIdx, currentFrame, group->id(), PRE));
    }
    key->selection().clearSelectedPreGroups();
    key->selection().clearSelectedTrajectory();
    // m_undoStack->push(new SetSelectedGroupCommand(m_editor, layerIdx, currentFrame, -1));
    m_undoStack->endMacro();
    m_fixedSceneManager->updateKeyChart(key);
}

void Editor::deleteAllEmptyGroups() {
    int layerIdx = m_layerManager->currentLayerIndex();
    int currentFrame = m_playbackManager->currentFrame();
    deleteAllEmptyGroups(layerIdx, currentFrame);
}

void Editor::makeInbetweensDirty() {
    Layer *layer = m_layerManager->currentLayer();
    for (auto it = layer->keysBegin(); it != layer->keysEnd(); ++it) {
        it.value()->makeInbetweensDirty();
    }
}

void Editor::toggleDrawSplat(bool drawSplat) {
    // Update all strokes buffers
    if (!m_exporting && QOpenGLContext::currentContext() != m_tabletCanvas->context()) m_tabletCanvas->makeCurrent();
    for (int layerIndex = 0; layerIndex < m_layerManager->layersCount(); ++layerIndex) {
        Layer *layer = m_layerManager->layerAt(layerIndex);
        if (layer == nullptr) continue;
        for (auto keyframe = layer->keysBegin(); keyframe != layer->keysEnd(); ++keyframe) {
            keyframe.value()->updateBuffers();
        }
    }
    m_tabletCanvas->update();
}

/**
 * Add a new empty group and select it. If there is already an empty group, it is directly selected instead of adding a new empty group
 */
void Editor::drawInNewGroup() {
    int layerIdx = m_layerManager->currentLayerIndex();
    int currentFrame = m_playbackManager->currentFrame();
    VectorKeyFrame *key = prevKeyFrame();

    if (m_toolsManager->currentTool()->needReturnFocus()) return;

    deleteAllEmptyGroups();

    // qDebug() << "last group id: " << key->postGroups().lastGroup()->id();
    // qDebug() << "lasy key: " << key->postGroups().lastKey();

    if (key->postGroups().lastGroup() == nullptr || key->postGroups().lastGroup()->size() > 0) {
        m_undoStack->push(new AddGroupCommand(this, layerIdx, currentFrame));
    }

    m_undoStack->push(new SetSelectedGroupCommand(this, layerIdx, currentFrame, key->postGroups().lastGroup()->id()));
}

/**
 * 
 */
void Editor::suggestLayoutChange() {
    int layerIdx = m_layerManager->currentLayerIndex();
    int currentFrame = m_playbackManager->currentFrame();
    Layer *layer = m_layerManager->currentLayer();
    VectorKeyFrame *key = prevKeyFrame();


    if (key->nextKeyframe() != nullptr) {
        OrderPartial prevOrder = key->orderPartials().lastPartialAt(alpha(currentFrame));
        GroupOrder order(key);
        double score = m_layoutManager->computeBestLayout(key, key->nextKeyframe(), order);
        if (score >= 0.0) {
            qDebug() << "OPTIMAL LAYOUT CHANGE DETECTED | Score = " << score;
            StopWatch s("Compute best layout");
            int optimalInbetween = m_layoutManager->computeBestLayoutChangeLocation(key, order);
            s.stop();
            int stride = layer->stride(currentFrame);
            double dt = 1.0 / stride;
            double partialAlpha = (optimalInbetween - 0.5) * dt;
            qDebug() << "Optimal t = " << partialAlpha;
            order.setParentKeyFrame(key);
            key->orderPartials().insertPartial(OrderPartial(key, partialAlpha, order)); // TODO qcommand
            m_undoStack->push(new AddOrderPartial(this, layerIdx, currentFrame, OrderPartial(key, partialAlpha, order), prevOrder));
            scrubTo(key->keyframeNumber() + optimalInbetween);
            m_tabletCanvas->showInfoMessage(QString("Layout change found at frame #%1").arg(key->keyframeNumber() + optimalInbetween), 2000);
        } else {
            m_tabletCanvas->showInfoMessage("No layout change found", 2000);
        }
    }

    m_toolsManager->setTool(Tool::GroupOrdering);

    // TODO display success/fail text on the canvas?
}

void Editor::propagateLayoutForward() {
    int layerIdx = m_layerManager->currentLayerIndex();
    int currentFrame = m_playbackManager->currentFrame();
    VectorKeyFrame *key = prevKeyFrame();

    if (key->nextKeyframe() != nullptr) {
        StopWatch s("Propagate layout forward");
        GroupOrder order = m_layoutManager->propagateLayoutAtoB(key, key->nextKeyframe());
        s.stop();
        key->nextKeyframe()->orderPartials().insertPartial(OrderPartial(key->nextKeyframe(), 0.0, order));
        m_tabletCanvas->showInfoMessage("Layout propagated forward", 2000);
    }
}

void Editor::propagateLayoutBackward() {
    int layerIdx = m_layerManager->currentLayerIndex();
    int currentFrame = m_playbackManager->currentFrame();
    VectorKeyFrame *key = prevKeyFrame();

    if (key->prevKeyframe() != nullptr && key->prevKeyframe() != key) {
        GroupOrder order = m_layoutManager->propagateLayoutBtoA(key->prevKeyframe(), key);
        order.debug();
        key->prevKeyframe()->orderPartials().insertPartial(OrderPartial(key->prevKeyframe(), 0.0, order));
        m_tabletCanvas->showInfoMessage("Layout propagated backward", 2000);
    }
}

void Editor::suggestVisibilityThresholds() {
    LocalMaskTool *tool = static_cast<LocalMaskTool *>(m_toolsManager->tool(Tool::LocalMask));

    if (!tool->validatingClusters()) {
        m_undoStack->push(new ComputeVisibilityCommand(this, m_layerManager->currentLayerIndex(), m_playbackManager->currentFrame()));
        m_toolsManager->setTool(Tool::LocalMask);
        tool->setValidingCusters(true);
    } else {
        tool->setValidingCusters(false);
    }
}

void Editor::debugReport() {
    m_tabletCanvas->debugReport();
}