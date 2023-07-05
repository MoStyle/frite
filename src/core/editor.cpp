/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

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
#include "canvasscenemanager.h"
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
#include "arap.h"
#include "utils/stopwatch.h"

static dkBool k_autoBreak("Options->Layers->Auto-Break", true);
dkSlider k_deformRange("Warp->Range of deformation", 75.0f, 1.0f, 100.0f, 2.0f);
static dkBool k_exportGrid("Options->Export->Draw grid", false);
static dkInt k_regularizationIt("Options->Grid->Manual regularization itetations", 100, 0, 1000, 1);
extern dkBool k_exportOnlyCurSegment;
extern dkInt k_exportFrom;
extern dkInt k_registrationRegularizationIt;

extern dkBool k_debug_out;
extern dkBool k_exportOnionSkinMode;
extern dkBool k_useDeformAsSource;


static VectorKeyFrame *g_clipboardVectorKeyFrame = nullptr;

Editor::Editor(QObject *parent) : QObject(parent) { m_clipboardBitmapOk = false; }

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
    m_canvasSceneManager = new CanvasSceneManager(this);
    m_fixedSceneManager = new FixedSceneManager(this);
    m_selectionManager = new SelectionManager(this);

    m_layerManager->setEditor(this);
    m_playbackManager->setEditor(this);
    m_viewManager->setEditor(this);
    m_gridManager->setEditor(this);
    m_registrationManager->setEditor(this);
    m_toolsManager->setEditor(this);
    m_canvasSceneManager->setEditor(this);
    m_fixedSceneManager->setEditor(this);
    m_selectionManager->setEditor(this);

    connect(m_toolsManager, SIGNAL(toolChanged(Tool *)), m_canvasSceneManager, SLOT(toolChanged(Tool *)));
    connect(this, SIGNAL(currentFrameChanged(int)), m_canvasSceneManager, SLOT(frameChanged(int)));
    connect(this, SIGNAL(currentFrameChanged(int)), m_fixedSceneManager, SLOT(frameChanged(int)));
    connect(this, SIGNAL(timelineUpdate(int)), m_fixedSceneManager, SLOT(frameChanged(int)));
    connect(this, SIGNAL(timelineUpdate(int)), m_canvasSceneManager, SLOT(frameChanged(int)));

    m_undoStack = new QUndoStack(this);
    connect(m_undoStack, &QUndoStack::indexChanged, this, &Editor::updateTimeLine);

    setTabletCanvas(canvas);

    m_toolsManager->initTools();
    m_canvasSceneManager->setScene(m_tabletCanvas->graphicsScene());
    m_fixedSceneManager->setScene(m_tabletCanvas->fixedGraphicsScene());

    return true;
}

void Editor::setTabletCanvas(TabletCanvas *canvas) {
    m_tabletCanvas = canvas;
    connect(m_undoStack, &QUndoStack::indexChanged, m_tabletCanvas, &TabletCanvas::updateCurrentFrame);
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

    if (!m_layerManager->load(element, path)) return false;

    emit currentFrameChanged(playback()->currentFrame());
    QCoreApplication::processEvents(); // not very elegent way but we need the previous to be processed immediately 

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

// !TODO
void Editor::copy() {
    qDebug() << "COPY";
    // Layer *layer = m_layerManager->layerAt(layers()->currentLayerIndex());
    // if (layer != nullptr) {
    //     if (m_tabletCanvas->somethingSelected()) {
    //         g_clipboardVectorKeyFrame = layer->getLastVectorKeyFrameAtFrame(m_playbackManager->currentFrame(),
    //                                                                         0)
    //                                         ->copy(m_tabletCanvas->getSelection()->bounds());  // copy part of the
    //                                         image
    //     } else {
    //         g_clipboardVectorKeyFrame = layer->getLastVectorKeyFrameAtFrame(m_playbackManager->currentFrame(), 0)
    //                                         ->copy();  // copy the whole image
    //     }
    //     m_clipboardBitmapOk = true;
    // }
}

void Editor::paste() {
    Layer *layer = m_layerManager->layerAt(layers()->currentLayerIndex());
    if (layer != nullptr) {
        if (g_clipboardVectorKeyFrame != nullptr) {
            m_undoStack->beginMacro("Paste");
            int prevkey = m_playbackManager->currentFrame();
            if (!layer->keyExists(m_playbackManager->currentFrame())) {
                if (k_autoBreak) {
                    addKey();
                } else {
                    prevkey = layer->getPreviousKeyFramePosition(m_playbackManager->currentFrame());
                    if (m_playbackManager->currentFrame() >= layer->getMaxKeyFramePosition())
                        m_undoStack->push(new MoveKeyCommand(this, m_layerManager->currentLayerIndex(), layer->getMaxKeyFramePosition(),
                                                             m_playbackManager->currentFrame() + 1));
                }
            }
            if (layer->keyExists(prevkey)) {
                m_undoStack->push(new PasteCommand(this, layers()->currentLayerIndex(), prevkey, g_clipboardVectorKeyFrame));
            }
            m_undoStack->endMacro();
        }
    }
}

// !TODO
void Editor::deselectAll() { 

}

qreal Editor::alpha(int frame) {
    Layer *layer = m_layerManager->currentLayer();
    if (layer) {
        if (frame >= layer->getMaxKeyFramePosition()) return 1.0;

        int prevKey;
        if (layer->keyExists(frame))
            prevKey = frame;
        else
            prevKey = layer->getPreviousKeyFramePosition(frame);
        int nextKey = layer->getNextKeyFramePosition(frame);
        // nextKey -= 1;
        if (nextKey == prevKey + 1) return 0.0;
        return qreal(frame - prevKey) / (nextKey - prevKey);
    }
    return 0.0;
}

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
 * @param inbetween The relative index of the inbetween to update (1 <= inbetween <= stride)
 * @param stride The number of frames between the keyframe and the next one
*/
int Editor::updateInbetweens(VectorKeyFrame *keyframe, int inbetween, int stride) {
    if (inbetween > stride) inbetween = stride;
    if (keyframe->inbetweens().empty() || stride != keyframe->inbetweens().size()) {
        // qDebug() << "Inbetweens size: " << keyframe->inbetweens().size() << "  != stride: " << stride;
        keyframe->clearInbetweens();
        keyframe->initInbetweens(stride);
    }
    if (stride == 0 || inbetween <= 0) return inbetween;
    if (!m_exporting && QOpenGLContext::currentContext() != m_tabletCanvas->context()) m_tabletCanvas->makeCurrent();
    keyframe->bakeInbetween(this, keyframe->parentLayer()->getVectorKeyFramePosition(keyframe), inbetween, stride);
    return inbetween;
}

void Editor::exportFrames(const QString &path, QSize exportSize, bool transparency) {
    QFileInfo info(path);
    int maxFrame = m_layerManager->maxFrame()-1;
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
    
    // Destroy buffers made with the OpenGLCanvas default FBO
    for (Layer *layer : m_layerManager->layers()) {
        for (auto it = layer->keysBegin(); it != layer->keysEnd(); ++it) {
            it.value()->destroyBuffers();
        }
    }

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
            m_tabletCanvas->paintGLInit(exportSize.width(), exportSize.height(), true);
            m_tabletCanvas->drawCanvas(painter, true, true);
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
                float alphaLinear = alpha(frame);
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
            m_tabletCanvas->paintGLInit(exportSize.width(), exportSize.height(), true);
            m_tabletCanvas->drawCanvas(painter, true, true);
            m_tabletCanvas->paintGLRelease(true);
            painter.restore();

            QImage fboImage = m_tabletCanvas->grabCanvasFramebuffer();
            QImage image(fboImage.constBits(), fboImage.width(), fboImage.height(), QImage::Format_ARGB32);
            painter.drawImage(QPoint(0, 0), image); // the image is on top of vector drawings :(

            painter.save();
            painter.scale(scaleW, scaleH);
            painter.translate( m_tabletCanvas->canvasRect().width()/2,  m_tabletCanvas->canvasRect().height()/2);
            m_tabletCanvas->drawToolGizmos(painter);
            if (k_exportGrid) {
                Layer *layer = m_layerManager->currentLayer();
                VectorKeyFrame *keyframe = layer->getLastKey(frame);
                int inbetween = layer->inbetweenPosition(frame);
                int stride = layer->stride(frame);
                float alphaLinear = alpha(frame);

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
            m_tabletCanvas->fixedGraphicsScene()->render(&painter, targetRect);
            painter.restore();
            painter.end();

            img.save(info.absolutePath() + "/" + info.baseName() + "_" + frameNumberString + "." + info.completeSuffix());
        }
        std::cout << "Frame " << frame << " has been exported" << std::endl;
    }
    m_exporting = false;
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
    int prevkey = m_playbackManager->currentFrame();
    // Drawing on a non-keyframe (need to add a keyframe first)
    if (!layer->keyExists(prevkey) || prevkey == layer->getMaxKeyFramePosition()) {
        if (k_autoBreak) {
            addKey();
        } else {
            prevkey = layer->getPreviousKeyFramePosition(m_playbackManager->currentFrame());
            if (m_playbackManager->currentFrame() >= layer->getMaxKeyFramePosition())
                m_undoStack->push(new MoveKeyCommand(this, m_layerManager->currentLayerIndex(), layer->getMaxKeyFramePosition(),
                                                        m_playbackManager->currentFrame() + 1));
        }
        // we can't set the id of the stroke before this point when we're drawing on a not yet existing keyframe
        VectorKeyFrame *keyframe = layer->getVectorKeyFrameAtFrame(prevkey);
        stroke->resetID(keyframe->pullMaxStrokeIdx());
        int group = Group::MAIN_GROUP_ID;
        GroupType type = POST;
        if (!keyframe->selection().selectedPostGroups().empty()) {
            group = keyframe->selection().selectedPostGroups().constBegin().value()->id();
            type = POST;
        } else if (!keyframe->selection().selectedPreGroups().empty()) {
            group = keyframe->selection().selectedPreGroups().constBegin().value()->id();
            type = PRE;
        }
        m_undoStack->push(new DrawCommand(this, m_layerManager->currentLayerIndex(), prevkey, stroke, group, true, type));

    } 
    // Drawing on an existing keyframe
    else { 
        VectorKeyFrame *keyframe = layer->getVectorKeyFrameAtFrame(prevkey);
        int group = Group::MAIN_GROUP_ID;
        GroupType type = POST;
        if (!keyframe->selection().selectedPostGroups().empty()) {
            group = keyframe->selection().selectedPostGroups().constBegin().value()->id();
            type = POST;
        } else if (!keyframe->selection().selectedPreGroups().empty()) {
            group = keyframe->selection().selectedPreGroups().constBegin().value()->id();
            type = PRE;
        } 
        mReWarp = true;
        m_undoStack->push(new DrawCommand(this, m_layerManager->currentLayerIndex(), prevkey, stroke, group, true, type));
    }
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
    m_canvasSceneManager->selectedGroupChanged(key->selection().selectedPostGroups());  // Selected group(s) border (UI)
    emit m_tabletCanvas->groupsModified(POST);                                          // Update the group list UI
    emit m_tabletCanvas->groupsModified(PRE);                                           // Update the group list UI
}

void Editor::deselectInAllLayers() {
    Layer *layer = m_layerManager->currentLayer();
    int layerIdx = m_layerManager->currentLayerIndex();
    for (auto it = layer->keysBegin(); it != layer->keysEnd(); ++it) {
        m_undoStack->beginMacro("Deselect All");
        m_undoStack->push(new SetSelectedGroupCommand(this, layerIdx, it.key(), Group::ERROR_ID));
        m_undoStack->push(new SetSelectedTrajectoryCommand(this, layerIdx, it.key(), nullptr));
        m_undoStack->endMacro();
    }
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

void Editor::regularizeLattice() {
    VectorKeyFrame *key = prevKeyFrame();
    for (Group *group : key->selection().selectedPostGroups()) {
        if (group->lattice() == nullptr) continue;
        Arap::regularizeLattice(*group->lattice(), k_useDeformAsSource ? DEFORM_POS : REF_POS, TARGET_POS, k_regularizationIt, true, false);
        group->lattice()->setArapDirty();
        key->makeInbetweensDirty();
    }
    m_tabletCanvas->update();
}

void Editor::registerFromRestPosition() {
    VectorKeyFrame *key = prevKeyFrame();
    bool registerToNextKeyframe = m_registrationManager->registrationTargetEmpty();
    if (registerToNextKeyframe) m_registrationManager->setRegistrationTarget(key->nextKeyframe());
    const QHash<int, Group *> &groups = key->selection().selectedPostGroups().empty() ? key->groups(POST) : key->selection().selectedPostGroups();
    bool multipleGroupsSelected = groups.size() > 1;
    if (multipleGroupsSelected) m_registrationManager->preRegistration(groups, TARGET_POS);
    for (Group *group : groups) {
        m_registrationManager->registration(group, TARGET_POS, TARGET_POS, !multipleGroupsSelected);
    }
    if (registerToNextKeyframe) m_registrationManager->clearRegistrationTarget();
    key->makeInbetweensDirty();
    m_tabletCanvas->update();
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

void Editor::expandGrid() {
    VectorKeyFrame *key = prevKeyFrame();
    for (Group *group : key->selection().selectedPostGroups()) {
        std::vector<int> newQuads;
        for (QuadPtr q : group->lattice()->quads()) q->setFlag(false);
        m_gridManager->addOneRing(group->lattice(), newQuads);
        m_gridManager->propagateDeformToOneRing(group->lattice(), newQuads);
        group->lattice()->setArapDirty();
        group->lattice()->setBackwardUVDirty(true);
    }
    key->makeInbetweensDirty();
    m_tabletCanvas->update();
}

void Editor::copyGroupToNextKeyFrame() {
    Layer *layer = m_layerManager->currentLayer();
    int currentFrame = m_playbackManager->currentFrame();
    VectorKeyFrame *key = prevKeyFrame();
    if (currentFrame == layer->getMaxKeyFramePosition() - 1) return;
    VectorKeyFrame *next = key->nextKeyframe();
    for (Group *group : key->selection().selectedPostGroups()) {
        key->copyDeformedGroup(next, group);
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

void Editor::bakeNextPreGroup() {
    VectorKeyFrame *key = prevKeyFrame();
    for (Group *group : key->selection().selectedPostGroups()) {
        key->makeNextPreGroup(this, group);
    }
    m_tabletCanvas->update();
}

void Editor::removeNextPreGroup() {
    VectorKeyFrame *key = prevKeyFrame();
    int layerIdx = m_layerManager->currentLayerIndex();
    int currentFrame = m_playbackManager->currentFrame();
    for (Group *group : key->selection().selectedPostGroups()) {
        m_undoStack->push(new RemoveCorrespondenceCommand(this, layerIdx, currentFrame, group->id()));
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
    m_canvasSceneManager->selectedGroupChanged(QHash<int, Group *>());
    m_fixedSceneManager->updateKeyChart(key);
}

void Editor::makeInbetweensDirty() {
    Layer *layer = m_layerManager->currentLayer();
    for (auto it = layer->keysBegin(); it != layer->keysEnd(); ++it) {
        it.value()->makeInbetweensDirty();
    }
}

void Editor::debugReport() {
    m_tabletCanvas->debugReport();
}