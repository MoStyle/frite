#include "vectorkeyframe.h"
#include "dialsandknobs.h"
#include "point.h"
#include "polyline.h"
#include "group.h"
#include "grouplist.h"
#include "editor.h"
#include "gridmanager.h"
#include "viewmanager.h"
#include "playbackmanager.h"
#include "canvascommands.h"
#include "keycommands.h"
#include "selectionmanager.h"
#include "layermanager.h"
#include "tabletcanvas.h"
#include "utils/utils.h"
#include "utils/stopwatch.h"
#include "qteigen.h"

#include <QtGui>
#include <limits>
#include <iostream>
#include <unordered_map>

using namespace std;

static dkBool k_smoothPressure("Pen->Smooth pressure", false);
static dkBool k_smoothPressureAfter("Pen->Smooth pressure after", false);
static dkBool k_resample("Pen->Resample stroke", true);
static dkFloat k_minSampling("Pen->Min sampling", 4.0, 0.01, 10.0, 0.01);
static dkFloat k_maxSampling("Pen->Max sampling", 5.0, 0.01, 10.0, 0.01);
static dkBool k_hideMainGroup("Options->Drawing->Hide main group", false);
static dkBool k_showPivot("RigidDeform->Show pivot", false);

dkBool k_useInterpolation("Options->Drawing->Show Interpolation", true);
dkBool k_useCrossFade("Options->Drawing->Show Cross Fade", true);

extern dkBool k_drawSplat;
extern dkBool k_displayMask;
extern dkInt k_cellSize;
extern dkBool k_useJitter;
extern dkSlider k_jitterTranslation;
extern dkFloat k_jitterRotation;
extern dkInt k_jitterDuration;

VectorKeyFrame::VectorKeyFrame(Layer *layer) 
    : m_layer(layer),
      m_currentGroupHue(0.0f),
      m_maxStrokeIdx(0),
      m_orderPartials(this, OrderPartial(this, 0.0, GroupOrder(this))),
      m_preGroups(PRE, this),
      m_postGroups(POST, this),
      m_selection(this),
      m_pivotCurve(nullptr),
      m_pivot(new KeyframedVector("Pivot")),
      m_pivotTranslationExtracted(false),
	  m_pivotRotationExtracted(false),
      m_transform(new KeyframedTransform("Transform")),
      m_spacing(new KeyframedReal("Spacing")),
      m_alignTangentStart(false, Point::VectorType(1, 0)),
      m_alignTangentEnd(false, Point::VectorType(1, 0)),
      m_maxConstraintIdx(0) {
    resetRigidDeformation();
}

VectorKeyFrame::~VectorKeyFrame() { 
    clear(); 
    delete m_transform;
    delete m_spacing;
}

void VectorKeyFrame::clear() {
    // clear selection
    m_selection.clearAll();

    // clear correspondences
    m_correspondences.clear();
    m_intraCorrespondences.clear();

    // delete post groups
    for (Group *group : m_postGroups) {
        delete group;
    }
    m_postGroups.clear();

    // delete pre groups
    for (Group *group : m_preGroups) {
        delete group;
    }
    m_preGroups.clear();

    // delete strokes
    destroyBuffers();
    m_strokes.clear();
    m_visibility.clear();
    m_bounds = QRectF();

    // clear order partials
    m_orderPartials = Partials<OrderPartial>(this, OrderPartial(this, 0.0, GroupOrder(this)));

    // clear trajectories
    m_trajectories.clear();

    // reset properties
    m_currentGroupHue = 0.0f;
    m_maxStrokeIdx = 0;
    m_maxConstraintIdx = 0;

    // reset all deformations
    // resetRigidDeformation();
    // restore default group
    m_postGroups.add(new Group(this, QColor(Qt::black), MAIN));
}

StrokePtr VectorKeyFrame::addStroke(const StrokePtr &stroke, Group *group, bool resample) {
    if (m_strokes.contains(stroke->id())) {
        qCritical() << "Error! This keyframe already has a stroke with the id: " << stroke->id();
        return nullptr;
    }

    // if (k_smoothPressure) {
    //     stroke->smoothPressure();
    // }

    // resample stroke
    StrokePtr newStroke;

    if (k_resample && resample) {
        newStroke = stroke->resample(k_maxSampling, k_minSampling);
    } else {
        newStroke = stroke;
    }

    if (k_smoothPressureAfter) {
        newStroke->smoothPressure();
    }

    updateBounds(newStroke.get());
    newStroke->computeNormals();
    newStroke->computeOutline();

    m_strokes.insert(newStroke->id(), newStroke);

    if (group != nullptr) {
        group->addStroke(newStroke->id());
        for (int i = 0; i < newStroke->size(); ++i) {
            newStroke->points()[i]->setGroupId(group->id());
        }
    }

    return newStroke;
}

// should only be called in an undo/redo context! otherwise it might mess up stroke index
void VectorKeyFrame::removeLastStroke() {
    if (m_strokes.empty()) return;
    
    int maxKey = m_maxStrokeIdx - 1;
    while (maxKey >= 0) {
        if (m_strokes.contains(maxKey)) {
            break;
        }
        maxKey--;
    }

    const StrokePtr &stroke = m_strokes.value(maxKey);

    // TODO: to avoid iterating through all groups, or all stroke points, strokes could store the list of groups it belongs to (might be hard to keep up) 
    for (auto it = m_postGroups.begin(); it != m_postGroups.end(); ++it) (*it)->clearStrokes(stroke->id());
    for (auto it = m_preGroups.begin(); it != m_preGroups.end(); ++it) (*it)->clearStrokes(stroke->id());
    m_strokes.remove(stroke->id());
}

void VectorKeyFrame::removeStroke(Stroke *stroke, bool free) { 
    removeStroke(stroke->id(), free);
}

void VectorKeyFrame::removeStroke(unsigned int id, bool free) {
    if (!m_strokes.contains(id)) {
        qCritical() << "Error! Cannot remove remove stroke : idx" << id << " not in the hash!";
        return;
    }
    if (QOpenGLContext::currentContext() != m_layer->editor()->tabletCanvas()->context()) m_layer->editor()->tabletCanvas()->makeCurrent();
    m_strokes.value(id)->destroyBuffers();
    // TODO: to avoid iterating through all groups, or all stroke points, strokes could store the list of groups it belongs to (might be hard to keep up) 
    for (auto it = m_postGroups.begin(); it != m_postGroups.end(); ++it) (*it)->clearStrokes(id);
    for (auto it = m_preGroups.begin(); it != m_preGroups.end(); ++it) (*it)->clearStrokes(id);
    m_strokes.remove(id);
}

Stroke *VectorKeyFrame::stroke(unsigned int id) const {
    if (!m_strokes.contains(id)) {
        qCritical() << "Cannot find stroke with id " << id << ". MaxId is " << m_maxStrokeIdx;
        return nullptr;
    }
    return m_strokes.value(id).get();
}

void VectorKeyFrame::updateBuffers() {
    for (const StrokePtr &stroke : m_strokes) {
        stroke->updateBuffer(this);
    }
    makeInbetweensDirty();
}

void VectorKeyFrame::destroyBuffers() {
    if (QOpenGLContext::currentContext() != m_layer->editor()->tabletCanvas()->context()) m_layer->editor()->tabletCanvas()->makeCurrent();
    for (const StrokePtr &stroke : m_strokes) {
        stroke->destroyBuffers();
    }
    for (Inbetween &inbetween : m_inbetweens) {
        inbetween.destroyBuffers();
    }
}

/**
 * Fill the given inbetween structure based in the given interpolating alpha value.
 * An inbetween frame is made of 2 sets of strokes:
 *     - forward strokes coming from the previous keyframe
 *     - backward strokes coming from the next keyframe (if there is a correspondence)
 * This function takes as input an interpolating factor alpha in [0,1] and fills both sets of strokes and the interpolated grid corners.
 * Note that strokes opacity or thickness is not yet interpolated at this point, instead this is done during the rendering of the inbetween.
*/
void VectorKeyFrame::computeInbetween(qreal alpha, Inbetween &inbetween) const {
    Point::Affine rigidTrans = rigidTransform(alpha);
    inbetween.nbVertices = 0;

    // Copy forward strokes
    for (const StrokePtr &stroke : m_strokes) {
        inbetween.strokes.insert(stroke->id(), std::make_shared<Stroke>(*stroke));
    }

    // Compute the interpolated deformation of each group and use it to warp forward and backward strokes
    for (Group *group : m_postGroups) {
        if (group->size(alpha) > 0) {
            qreal spacing = group->spacingAlpha(alpha);
            if (group->lattice() == nullptr) return;
            // Interpolate the lattice if this is not done
            if (group->lattice()->isArapPrecomputeDirty()) {
                group->lattice()->precompute();
            }
            if (group->lattice()->isArapInterpDirty() || spacing != group->lattice()->currentPrecomputedTime()) {
                group->lattice()->interpolateARAP(alpha, spacing, rigidTransform(alpha));
            }
            // Use the interpolated lattice to compute the interpolated forward strokes
            Point::VectorType strokesCenterOfMass = Point::VectorType::Zero();
            // qDebug() << "group->uvs().size(): " << group->uvs().size();
            // qDebug() << "group->strokes(alpha).cbegin().key(): " << group->strokes(alpha).cbegin().key();
            // qDebug() << "group->strokes(alpha).cbegin().value().at(0).from(): " << group->strokes(alpha).cbegin().value().at(0).from();
            // qDebug() << "has: " << group->uvs().has(group->strokes(alpha).cbegin().key(), group->strokes(alpha).cbegin().value().at(0).from());
            UVInfo fuv = group->uvs().get(group->strokes(alpha).cbegin().key(), group->strokes(alpha).cbegin().value().at(0).from());
            Point::VectorType topLeft = group->lattice()->getWarpedPoint(inbetween.strokes[group->strokes(alpha).cbegin().key()]->points()[group->strokes(alpha).cbegin().value().at(0).from()]->pos(), fuv.quadKey, fuv.uv, INTERP_POS), bottomRight = topLeft;
            int nbPoints = 0;
            bool groupVisible = false;
            GLfloat visibility; // convert to GLfloat so that the comparison is the same as the one in the shaders
            GLfloat spacingFloat = (GLfloat)spacing;
            for (auto it = group->strokes(alpha).begin(); it != group->strokes(alpha).end(); ++it) {
                const StrokePtr &stroke = inbetween.strokes[it.key()]; 
                const Intervals &intervals = group->strokes(alpha).value(it.key());
                for (const Interval &interval : intervals) {
                    for (unsigned int i = interval.from(); i <= interval.to(); i++) {
                        UVInfo uv = group->uvs().get(it.key(), i);
                        stroke->points()[i]->pos() = group->lattice()->getWarpedPoint(stroke->points()[i]->pos(), uv.quadKey, uv.uv, INTERP_POS);
                        strokesCenterOfMass += stroke->points()[i]->pos();
                        if (stroke->points()[i]->pos().x() < topLeft.x()) topLeft.x() = stroke->points()[i]->pos().x();
                        else if (stroke->points()[i]->pos().x() > bottomRight.x()) bottomRight.x() = stroke->points()[i]->pos().x();
                        if (stroke->points()[i]->pos().y() > topLeft.y()) topLeft.y() = stroke->points()[i]->pos().y();
                        else if (stroke->points()[i]->pos().y() < bottomRight.y()) bottomRight.y() = stroke->points()[i]->pos().y();
                        if (!groupVisible) {
                            visibility = m_visibility.value(Utils::cantor(stroke->id(), i), 0.0);
                            if (visibility >= -1.0 && visibility != 0.0) {
                                visibility = Utils::sgn(visibility) * group->spacingAlpha(std::abs(visibility));
                            }
                            groupVisible = groupVisible || (visibility >= -1.0 && (visibility >= 0.0 ? spacingFloat >= visibility : -(spacingFloat) > visibility));
                        }
                        nbPoints++;
                        inbetween.nbVertices++;
                    }
                }
            }
            inbetween.aabbs.insert(group->id(), QRectF(EQ_POINT(topLeft), EQ_POINT(bottomRight)));
            inbetween.centerOfMass.insert(group->id(), strokesCenterOfMass / nbPoints);
            inbetween.fullyVisible.insert(group->id(), groupVisible);
            // Save the lattice interpolated corners (mainly for debugging)
            if (inbetween.corners.contains(group->id())) inbetween.corners[group->id()].clear();
            std::vector<Point::VectorType> &cornersPoint = inbetween.corners[group->id()];
            cornersPoint.resize(group->lattice()->corners().size());
            int idx = 0;
            for (Corner *corner : group->lattice()->corners()) {
                cornersPoint[idx] = corner->coord(INTERP_POS);
                idx++;
            }
        }

        // If there is a corresponding next group, copy and warp backward strokes
        if (m_correspondences.contains(group->id())) {
            Group *next = group->nextPreGroup();
            if (next->nextPostGroup() != nullptr && !next->nextPostGroup()->breakdown()) {
                for (auto it = next->strokes().begin(); it != next->strokes().end(); ++it) {
                    Stroke *stroke = next->stroke(it.key());
                    StrokePtr newStroke = std::make_shared<Stroke>(*stroke);
                    inbetween.backwardStrokes.insert(newStroke->id(), newStroke);
                    for (auto itIntervals = it.value().begin(); itIntervals != it.value().end(); ++itIntervals) {
                        qreal spacing = group->spacingAlpha(alpha);
                        if (group->lattice() == nullptr) return;
                        // Interpolate the lattice if this is not done
                        if (group->lattice()->isArapPrecomputeDirty()) group->lattice()->precompute();
                        if (group->lattice()->isArapInterpDirty() || spacing != group->lattice()->currentPrecomputedTime()) group->lattice()->interpolateARAP(alpha, spacing, group->globalRigidTransform(alpha));
                        // bake only the portion of the stroke inside a pre group
                        if (group->lattice()->backwardUVDirty()) group->lattice()->bakeBackwardUV(newStroke.get(), (*itIntervals), group->globalRigidTransform(alpha).inverse(), group->backwardUVs());
                        for (int i = itIntervals->from(); i <= itIntervals->to(); ++i) {
                            UVInfo uv = group->backwardUVs().get(newStroke->id(), i);
                            Point::VectorType prev = newStroke->points()[i]->pos(); 
                            newStroke->points()[i]->pos() = group->lattice()->getWarpedPoint(newStroke->points()[i]->pos(), uv.quadKey, uv.uv, INTERP_POS);
                        }
                    }
                }
                group->lattice()->setBackwardUVDirty(false);
            }
        }
    }
}

/**
 * Remove all cached inbetween frames.
 * Should be called in a valid OpenGL context!
*/
void VectorKeyFrame::clearInbetweens() {
    if (QOpenGLContext::currentContext() != m_layer->editor()->tabletCanvas()->context()) m_layer->editor()->tabletCanvas()->makeCurrent();
    for (Inbetween &inbetween : m_inbetweens) inbetween.destroyBuffers();
    m_inbetweens.clear(); 
    m_inbetweens.makeDirty(); 
}

/**
 * Create a list of empty inbetweens
*/
void VectorKeyFrame::initInbetweens(int stride) {
    for (int i = 0; i <= stride; ++i) {
        m_inbetweens.emplace_back(Inbetween());
        m_inbetweens.makeDirty();
    }
    qDebug() << "(Re)Initializing inbetweens";
}

/**
 * Compute and cache the inbetween frame 
*/
void VectorKeyFrame::bakeInbetween(Editor *editor, int frame, int inbetween, int stride) {
    if (inbetween > m_inbetweens.size()) {
        qCritical() << "Invalid inbetween vector size! (" << inbetween << " vs " << m_inbetweens.size() << ")";
        return;
    }
    
    if (m_inbetweens.isClean(inbetween)) return;

    if (stride <= 0 || inbetween > stride) return;

    qreal alphaLinear = editor->alpha(frame + inbetween, m_layer);
    if (alphaLinear == 0.0f && inbetween == stride) alphaLinear = 1.0f;

    m_inbetweens[inbetween].clear();
    computeInbetween(alphaLinear, m_inbetweens[inbetween]);
    m_inbetweens.makeClean(inbetween);

    qDebug() << "Baked " << inbetween << " (linear alpha = " << alphaLinear << ")";
}

void VectorKeyFrame::updateInbetween(Editor *editor, size_t i) {
    // TODO only update strokes that have changed
}

void VectorKeyFrame::addIntraCorrespondence(int preGroupId, int postGroupId) { 
    m_intraCorrespondences.insert(preGroupId, postGroupId); 
    m_postGroups.fromId(postGroupId)->setPrevPreGroupId(preGroupId); 
    m_postGroups.fromId(postGroupId)->setBreakdown(true); 
    m_preGroups.fromId(preGroupId)->setBreakdown(true); 
}

void VectorKeyFrame::removeIntraCorrespondence(int preGroupId) {
    if (m_intraCorrespondences.contains(preGroupId)) {
        m_postGroups.fromId(m_intraCorrespondences.value(preGroupId))->setBreakdown(false);
        m_postGroups.fromId(m_intraCorrespondences.value(preGroupId))->setPrevPreGroupId(-1);
    }
    if (m_preGroups.fromId(preGroupId)) m_preGroups.fromId(preGroupId)->setBreakdown(false);
    m_intraCorrespondences.remove(preGroupId);
}

void VectorKeyFrame::clearIntraCorrespondences() {
    m_intraCorrespondences.clear(); 
    for (Group *group : m_postGroups) {
        group->setBreakdown(false);
        group->setPrevPreGroupId(-1);
    }
}

void VectorKeyFrame::resetInterStrokes() {
    for(auto group : m_postGroups)
        group->resetInterStrokes();
}

QColor tintColor(const StrokePtr &stroke, float tintFactor, QColor color) {
    return QColor(int((stroke->color().redF() * (100.0 - tintFactor) + color.redF() * tintFactor) * 2.55),
                    int((stroke->color().greenF() * (100.0 - tintFactor) + color.greenF() * tintFactor) * 2.55),
                    int((stroke->color().blueF() * (100.0 - tintFactor) + color.blueF() * tintFactor) * 2.55), 255);
};

void VectorKeyFrame::paintGroupGL(QOpenGLShaderProgram *program, QOpenGLFunctions *functions, qreal alpha, double opacityAlpha, Group *group, int inbetween, const QColor &color, double tintFactor, double strokeWeightFactor, bool useGroupColor, bool crossFade, bool ignoreMask) {
    if (!k_useInterpolation) {
        alpha = 0.0f;
        inbetween = 0;
    }

    const QHash<int, StrokePtr> &strokes = m_inbetweens[inbetween].strokes;
    const StrokeIntervals &strokeIntervals = group->drawingPartials().lastPartialAt(alpha).strokes();
    Group *next = group->nextPreGroup();
    qreal spacingAlpha = group->spacingAlpha(alpha);
    bool drawNext = next != nullptr && crossFade && k_useCrossFade && next->nextPostGroup() != nullptr && !next->nextPostGroup()->breakdown() ;
    float widthScalingForward =  drawNext ? group->crossFadeValue(spacingAlpha, true)  : 1.0f;
    float widthScalingBackward = drawNext ? group->crossFadeValue(spacingAlpha, false) : 1.0f; 
    if (group->disappear()) widthScalingForward = std::max(1.0 - spacingAlpha, 0.0);
    if (drawNext && group->size() == 0) widthScalingBackward = std::max(spacingAlpha, 0.0);
    program->setUniformValue("ignoreMask", ignoreMask);
    program->setUniformValue("sticker", group->isSticker());
    program->setUniformValue("groupId", (GLint)group->id());
    program->setUniformValue("time", (GLfloat)spacingAlpha);
    program->setUniformValue("stride", (GLint)m_layer->stride(keyframeNumber()));

    // Draw forward strokes
    QColor colorAlpha;
    for (auto it = strokeIntervals.begin(); it != strokeIntervals.end(); ++it) {
        const StrokePtr &stroke = strokes.value(it.key());
        if (stroke->isInvisible() && !k_displayMask) continue;

        // Select stroke color
        if (!stroke->buffersCreated()) stroke->createBuffers(program, this);
        if (useGroupColor)          colorAlpha = group->color();
        else if (tintFactor > 0.0)  colorAlpha = tintColor(stroke, tintFactor, color);
        else                        colorAlpha = stroke->color();
        colorAlpha.setAlphaF(opacityAlpha);

        // Optional jitter
        unsigned int jitterId = std::floor((float)inbetween/k_jitterDuration);
        QTransform jitter;
        if (k_useJitter && inbetween > 0 && jitterId > 0) {
            srand(Utils::cantor(stroke->id(), jitterId));
            Point::VectorType strokeCentroid = stroke->centroid();
            jitter.translate(strokeCentroid.x() + static_cast<float>(rand())/(static_cast<float>(RAND_MAX/float(k_jitterTranslation))) - k_jitterTranslation * 0.5, strokeCentroid.y() + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX/float(k_jitterTranslation))) - k_jitterTranslation * 0.5);
            jitter.rotateRadians((static_cast<float>(rand())/(static_cast<float>(RAND_MAX))) * k_jitterRotation - (k_jitterRotation * 0.5f));
            jitter.translate(-strokeCentroid.x(), -strokeCentroid.y());
        }
        program->setUniformValue("jitter", jitter);
        
        // Stroke-wide properties
        program->setUniformValue("strokeWeight", (float)stroke->strokeWidth() * widthScalingForward * (float)strokeWeightFactor);
        program->setUniformValue("strokeColor", colorAlpha);

        // Draw stroke intervals
        for (const Interval &interval : it.value()) {
            if (!k_drawSplat) {
                int cap[2] = {(int)interval.from(), (int)interval.to()}; // TODO: do this more properly
                if (inbetween == 0 && interval.canOvershoot() && interval.to() < stroke->size() - 1) cap[1] += 1;
                program->setUniformValueArray("capIdx", cap, 2); // at which points should we draw caps
            }
            stroke->render(GL_LINE_STRIP_ADJACENCY, functions, interval, inbetween == 0);
        }
    }

    // draw backward strokes (if cross-fade is enabled)
    // TODO factorize with above
    if (drawNext && inbetween > 0) {
        for (auto it = next->strokes().begin(); it != next->strokes().end(); ++it) {
            const StrokePtr &stroke = m_inbetweens[inbetween].backwardStrokes.value(it.key());
            if (stroke->isInvisible()) continue;
            if (!stroke->buffersCreated()) stroke->createBuffers(program, this);
            if (useGroupColor)          colorAlpha = group->color();
            else if (tintFactor > 0.0)  colorAlpha = tintColor(stroke, tintFactor, color);
            else                        colorAlpha = stroke->color();
            colorAlpha.setAlphaF(opacityAlpha);
            program->setUniformValue("strokeWeight", (float)stroke->strokeWidth() * widthScalingBackward * (float)strokeWeightFactor);
            program->setUniformValue("strokeColor", colorAlpha);
            for (const Interval &interval : it.value()) {
                if (!k_drawSplat) {
                    int cap[2] = {(int)interval.from(), (int)interval.to()}; // TODO: do this more properly
                    program->setUniformValueArray("capIdx", cap, 2);
                }
                stroke->render(GL_LINE_STRIP_ADJACENCY, functions, interval, false);
            }
        }
    }  
}

void VectorKeyFrame::paintGroupGL(QOpenGLShaderProgram *program, QOpenGLFunctions *functions, double opacityAlpha, Group *group, const QColor &color, double tintFactor, double strokeWeightFactor, bool useGroupColor, bool ignoreMask) {
    const StrokeIntervals &strokeIntervals = group->strokes();
    program->setUniformValue("ignoreMask", ignoreMask);
    program->setUniformValue("groupId", (GLint)group->id());
    program->setUniformValue("sticker", group->isSticker());
    program->setUniformValue("time", (GLfloat)0.0);
    program->setUniformValue("stride", (GLint)m_layer->stride(keyframeNumber()));
    for (auto it = strokeIntervals.begin(); it != strokeIntervals.end(); ++it) {
        const StrokePtr &stroke = m_strokes.value(it.key());
        if (stroke->isInvisible()) continue;
        if (!stroke->buffersCreated()) stroke->createBuffers(program, this);
        QColor colorAlpha;
        if (useGroupColor)          colorAlpha = group->color();
        else if (tintFactor > 0.0)  colorAlpha = tintColor(stroke, tintFactor, color);
        else                        colorAlpha = stroke->color();
        colorAlpha.setAlphaF(opacityAlpha);
        srand(Utils::cantor(stroke->id(), 0));
        QTransform jitter;
        program->setUniformValue("jitter", jitter);
        program->setUniformValue("strokeWeight", (float)stroke->strokeWidth() * (float)strokeWeightFactor * 2.0f);
        program->setUniformValue("strokeColor", colorAlpha);
        for (const Interval &interval : it.value()) {
            if (!k_drawSplat) {
                int cap[2] = {(int)interval.from(), (int)interval.to()};
                if (interval.canOvershoot() && interval.to() < stroke->size() - 1) cap[1] += 1;
                program->setUniformValueArray("capIdx", cap, 2);
            }
            stroke->render(GL_LINE_STRIP_ADJACENCY, functions, interval, true);
        }
    }
}

bool VectorKeyFrame::load(QDomElement &element, const QString &path, Editor *editor) {
    Q_UNUSED(path);

    // load strokes
    unsigned int maxId = 0;
    QDomElement strokesElt = element.firstChildElement("strokes");
    m_strokes.reserve(strokesElt.attribute("size").toUInt());
    if (!strokesElt.isNull()) {
        QDomNode strokeTag = strokesElt.firstChild();
        while (!strokeTag.isNull()) {
            unsigned int strokeId = strokeTag.toElement().attribute("id").toInt();
            if (strokeId > maxId) maxId = strokeId;
            QRgb color = strokeTag.toElement().attribute("color").toUInt(nullptr, 16);
            QColor c(color);
            double thickness = strokeTag.toElement().attribute("thickness", "1.5").toDouble();
            bool invisible = (bool)strokeTag.toElement().attribute("invisible", "0").toInt();
            StrokePtr s = std::make_shared<Stroke>(strokeId, c, thickness, invisible);
            uint size = strokeTag.toElement().attribute("size").toUInt();
            QString string = strokeTag.toElement().text();
            QTextStream pos(&string);
            s->load(pos, size);
            addStroke(s, nullptr, false);
            strokeTag = strokeTag.nextSibling();
        }
    }
    m_maxStrokeIdx = maxId + 1;

    // load post groups
    QDomElement postGroupsElt = strokesElt.nextSiblingElement("postgroups");
    if (!postGroupsElt.isNull()) {
        QDomNode groupNode = postGroupsElt.firstChild();
        while (!groupNode.isNull()) {
            if (groupNode.toElement().attribute("id").toInt() == Group::MAIN_GROUP_ID) {
                defaultGroup()->load(groupNode);
                defaultGroup()->update();
            } else {
                Group *group = new Group(this, POST);
                group->load(groupNode);
                group->update();
                m_postGroups.add(group);
            }
            groupNode = groupNode.nextSibling();
        }
    } else { // backward compatibility: load strokes directly into a single group        
        for (const StrokePtr &stroke : m_strokes) {
            defaultGroup()->addStroke(stroke->id());
        }
        if (defaultGroup()->lattice() == nullptr) {
            editor->grid()->constructGrid(defaultGroup(), editor->view(), k_cellSize);
        }
        defaultGroup()->update();
    }

    // load pre groups
    QDomElement preGroupsElt = strokesElt.nextSiblingElement("pregroups");
    if (!preGroupsElt.isNull()) {
        QDomNode groupNode = preGroupsElt.firstChild();
        while (!groupNode.isNull()) {
            Group *group = new Group(this, PRE);
            group->load(groupNode);
            group->update();
            m_preGroups.add(group);
            groupNode = groupNode.nextSibling();
        }
    }

    // load default group (retrocomp)
    QDomElement mainGroupElt = strokesElt.nextSiblingElement("maingroup");
    if (!mainGroupElt.isNull()) {
        QDomNode groupNode = mainGroupElt.firstChild();
        defaultGroup()->load(groupNode);
        editor->grid()->constructGrid(defaultGroup(), editor->view(), k_cellSize);
        defaultGroup()->update();
    }

    // load stroke visibility
    QDomElement strokeVisibilityElt = strokesElt.nextSiblingElement("strokevisibility");
    if (!strokeVisibilityElt.isNull()) {
        int size = strokeVisibilityElt.attribute("size", "0").toInt();
        QString stringVis = strokeVisibilityElt.text();
        QTextStream posVis(&stringVis);
        unsigned int key;
        double vis;
        for (int i = 0; i < size; ++i) {
            posVis >> key >> vis;
            m_visibility[key] = vis;
        }
    }

    // load correspondences
    QDomElement correspondences = strokesElt.nextSiblingElement("corresp");
    if (correspondences.isNull()) qDebug() << "Loading: could not find correspondences";
    int size = correspondences.attribute("size", "0").toInt();
    QString string = correspondences.text();
    QTextStream pos(&string);
    int groupA, groupB;
    for (int i = 0; i < size; ++i) {
        pos >> groupA >> groupB;
        addCorrespondence(groupA, groupB);
    }

    // load intra-correspondences
    QDomElement intraCorrespondences = strokesElt.nextSiblingElement("intra_corresp");
    if (intraCorrespondences.isNull()) qDebug() << "Loading: could not find intra correspondences";
    size = intraCorrespondences.attribute("size", "0").toInt();
    QString stringIntra = intraCorrespondences.text();
    QTextStream posIntra(&stringIntra);
    for (int i = 0; i < size; ++i) {
        posIntra >> groupA >> groupB;
        addIntraCorrespondence(groupA, groupB);
    }

    // Restore grid-stroke correspondence
    for (Group *group : m_postGroups) {
        bool uvPrecomputed = group->uvs().size() > 0;
        for (auto it = group->strokes().begin(); it != group->strokes().end(); ++it) {
            for (Interval &interval : it.value()) {
                const StrokePtr &stroke = m_strokes[it.key()];
                if (uvPrecomputed) {
                    group->lattice()->bakeForwardUVPrecomputed(stroke.get(), interval, group->uvs());
                    editor->grid()->bakeStrokeInGridPrecomputed(group->lattice(), group, stroke.get(), interval.from(), interval.to());
                } else {
                    group->lattice()->bakeForwardUV(stroke.get(), interval, group->uvs());
                    editor->grid()->bakeStrokeInGrid(group->lattice(), stroke.get(), interval.from(), interval.to());
                }
            }
        }

        if (group->lattice() !=  nullptr &&  group->lattice()->needRetrocomp()) {
            editor->grid()->retrocomp(group);
        }
    }
    
    // load global rigid trajectory
    QDomElement pivotElt = strokesElt.nextSiblingElement("pivot");
    m_pivot->set(Point::VectorType(pivotElt.attribute("px").toFloat(), pivotElt.attribute("py").toFloat()));
    m_pivot->addKey("Pivot", 0);
    m_pivot->addKey("Pivot", 1);
    QDomElement translationElt = strokesElt.nextSiblingElement("translation");
    if (!translationElt.isNull()) m_transform->translation.load(translationElt);
    QDomElement rotationElt = strokesElt.nextSiblingElement("rotation");
    if (!rotationElt.isNull()) m_transform->rotation.load(rotationElt);
    QDomElement spacingElt = strokesElt.nextSiblingElement("spacing");
    if (!spacingElt.isNull()) m_spacing->load(spacingElt);
    QDomElement rigidTransformElt = strokesElt.nextSiblingElement("rigidTransform");
    if (!rigidTransformElt.isNull()) m_transform->load(rigidTransformElt);

    // pivot parameters
    m_pivotTranslationExtracted = element.attribute("pivottranslation") != "0";
    m_pivotRotationExtracted = element.attribute("pivotrotation") != "0";

    QDomElement alignStartElt = strokesElt.nextSiblingElement("alignstart");
    QDomElement alignEndElt = strokesElt.nextSiblingElement("alignEnd");
    m_alignTangentStart.m_use = alignStartElt.attribute("used") != "0";
    m_alignTangentEnd.m_use = alignEndElt.attribute("used") != "0";

    QString stringStart = alignStartElt.text();
    QTextStream streamStart(&stringStart);
    Point::Scalar x, y;
    streamStart >> x >> y;
    m_alignTangentStart.m_axis = Point::VectorType(x, y);

    QString stringEnd = alignEndElt.text();
    QTextStream streamEnd(&stringEnd);
    streamEnd >> x >> y;
    m_alignTangentEnd.m_axis = Point::VectorType(x, y);

    // backward compatibility with old spacing curve
    if (m_spacing->curve()->interpType() != Curve::MONOTONIC_CUBIC_INTERP) {
        if (m_spacing->curve()->nbPoints() == 2) {
            for (int i = 1; i < 4; ++i) {
                m_spacing->frameChanged(i / 4.0f);
                m_spacing->addKey("Spacing", i / 4.0f);
            }
        }
        m_spacing->setInterpolation("Spacing", Curve::MONOTONIC_CUBIC_INTERP);
    }

    // load trajectories
    QDomElement trajsElt = strokesElt.nextSiblingElement("trajs");
    maxId = 0;
    if (!trajsElt.isNull()) {
        QDomNode trajNode = trajsElt.firstChild();
        while (!trajNode.isNull()) {
            QDomElement trajElt = trajNode.toElement();
            std::shared_ptr<Trajectory> traj = Trajectory::load(trajElt, this);
            m_trajectories.insert(traj->constraintID(), traj);
            traj->group()->lattice()->addConstraint(traj->constraintID());
            traj->group()->setGridDirty();
            if (traj->constraintID() > maxId) maxId = traj->constraintID();
            trajNode = trajNode.nextSibling();
        }
        // restore parent and children pointers
        for (const std::shared_ptr<Trajectory> &traj : m_trajectories) {
            if (traj->parentTrajectoryID() >= 0) traj->setParent(m_trajectories.value(traj->parentTrajectoryID()));
            for (int childId : traj->childrenTrajectoriesIds()) {
                traj->addChild(m_trajectories.value(childId));
            }
        }
    }
    m_maxConstraintIdx = maxId + 1;

    // load group order (retrocomp)
    QDomElement groupOrderElt = strokesElt.nextSiblingElement("group_order");
    if (!groupOrderElt.isNull()) {
        m_orderPartials.firstPartial().groupOrder().load(groupOrderElt);
    }

    QDomElement orderPartialsEl = strokesElt.nextSiblingElement("partials_group_order");
    if (!orderPartialsEl.isNull()) {
        m_orderPartials.load(orderPartialsEl);
    }

    return true;
}

bool VectorKeyFrame::save(QDomDocument &doc, QDomElement &root, const QString &path, int layer, int frame) const {
    Q_UNUSED(path);
    Q_UNUSED(layer);
    QDomElement keyElt = doc.createElement("vectorkeyframe");
    keyElt.setAttribute("frame", frame);

    // save strokes
    QDomElement strokesElt = doc.createElement("strokes");
    strokesElt.setAttribute("size", uint(m_strokes.size()));
    for (const StrokePtr &stroke : m_strokes) {
        stroke->save(doc, strokesElt);
    }
    keyElt.appendChild(strokesElt);

    // save post groups
    QDomElement postGroupsElt = doc.createElement("postgroups");
    postGroupsElt.setAttribute("size", uint(m_postGroups.size()));
    for (const Group *group : m_postGroups) {
        group->save(doc, postGroupsElt);
    }
    keyElt.appendChild(postGroupsElt);

    // save pre groups
    QDomElement preGroupsElt = doc.createElement("pregroups");
    preGroupsElt.setAttribute("size", uint(m_preGroups.size()));
    for (const Group *group : m_preGroups) {
        group->save(doc, preGroupsElt);
    }
    keyElt.appendChild(preGroupsElt);

    // save stroke visibility
    QDomElement strokeVisibilityElt = doc.createElement("strokevisibility");
    strokeVisibilityElt.setAttribute("size", uint(m_visibility.size()));
    QString stringVis;
    QTextStream startPosVis(&stringVis);
    for (auto it = m_visibility.constBegin(); it != m_visibility.constEnd(); ++it) {
        startPosVis << it.key() << " " << it.value() << " ";
    }
    QDomText txt = doc.createTextNode(stringVis);
    txt = doc.createTextNode(stringVis);
    strokeVisibilityElt.appendChild(txt);
    keyElt.appendChild(strokeVisibilityElt);

    // save global rigid trajectory
    QDomElement pivotElt = doc.createElement("pivot");
    m_pivot->frameChanged(0);
    Point::VectorType pivot = m_pivot->get();
    pivotElt.setAttribute("px", pivot.x());
    pivotElt.setAttribute("py", pivot.y());
    keyElt.appendChild(pivotElt);
    QDomElement rigidTransformElt = doc.createElement("rigidTransform");
    m_transform->save(doc, rigidTransformElt);
    keyElt.appendChild(rigidTransformElt);
    QDomElement spacingElt = doc.createElement("spacing");
    m_spacing->save(doc, spacingElt);
    keyElt.appendChild(spacingElt);

    // pivot parameters
    keyElt.setAttribute("pivottranslation", m_pivotTranslationExtracted ? "1" : "0");
    keyElt.setAttribute("pivotrotation", m_pivotRotationExtracted ? "1" : "0");
    QDomElement alignStartElt = doc.createElement("alignstart");
    QDomElement alignEndElt = doc.createElement("alignEnd");
    alignStartElt.setAttribute("used", m_alignTangentStart.m_use ? "1" : "0");
    alignEndElt.setAttribute("used", m_alignTangentEnd.m_use ? "1" : "0");

    QString stringStart;
    QTextStream streamStart(&stringStart);
    streamStart << m_alignTangentStart.m_axis.x() << " " << m_alignTangentStart.m_axis.y() << " ";
    txt = doc.createTextNode(stringStart);
    alignStartElt.appendChild(txt);
    keyElt.appendChild(alignStartElt);

    QString stringEnd;
    QTextStream streamEnd(&stringEnd);
    streamEnd << m_alignTangentEnd.m_axis.x() << " " << m_alignTangentEnd.m_axis.y() << " ";
    txt = doc.createTextNode(stringEnd);
    alignEndElt.appendChild(txt);
    keyElt.appendChild(alignEndElt);

    // save correspondences
    QDomElement correspondencesElt = doc.createElement("corresp");
    correspondencesElt.setAttribute("size", (int)m_correspondences.size());
    QString string;
    QTextStream startPos(&string);
    for (auto it = m_correspondences.constBegin(); it != m_correspondences.constEnd(); ++it) {
        startPos << it.key() << " " << it.value() << " ";
    }
    txt = doc.createTextNode(string);
    correspondencesElt.appendChild(txt);
    keyElt.appendChild(correspondencesElt);

    // save intra-correspondences
    QDomElement intraCorrespondencesElt = doc.createElement("intra_corresp");
    intraCorrespondencesElt.setAttribute("size", (int)m_intraCorrespondences.size());
    QString stringIntra;
    QTextStream startPosIntra(&stringIntra);
    for (auto it = m_intraCorrespondences.constBegin(); it != m_intraCorrespondences.constEnd(); ++it) {
        startPosIntra << it.key() << " " << it.value() << " ";
    }
    txt = doc.createTextNode(stringIntra);
    intraCorrespondencesElt.appendChild(txt);
    keyElt.appendChild(intraCorrespondencesElt);


    // save trajectories
    QDomElement trajsElt = doc.createElement("trajs");
    trajsElt.setAttribute("size", (int)m_trajectories.size());
    for (auto it = m_trajectories.constBegin(); it != m_trajectories.constEnd(); ++it) {
        it.value()->save(doc, trajsElt, this);
    }
    keyElt.appendChild(trajsElt);

    // save group order
    // QDomElement groupOrderElt = doc.createElement("group_order");
    // m_orderPartials.firstPartial().groupOrder().save(groupOrderElt);
    // keyElt.appendChild(groupOrderElt);

    // save group order partials
    QDomElement orderPartialsElt = doc.createElement("partials_group_order");
    m_orderPartials.save(doc, orderPartialsElt);
    keyElt.appendChild(orderPartialsElt);

    root.appendChild(keyElt);
    return true;
}
void VectorKeyFrame::setAlignFrameToTangent(bool start, AlignTangent alignTangent){
    if (start)
        m_alignTangentStart = alignTangent;
    else
        m_alignTangentEnd = alignTangent;
}

float VectorKeyFrame::getFrameRotation(float t) const{
    m_transform->frameChanged(t);
    Point::VectorType tangent = m_pivotCurve != nullptr ? m_pivotCurve->evalDer(t) : Point::VectorType::Zero();
    float frameRotationStart = 0.0f;
    float frameRotationEnd = 0.0f;
    if (m_alignTangentStart.m_use){
        Point::VectorType axis = m_alignTangentStart.m_axis;
        Point::Rotation rotation(- std::atan2(axis.y(), axis.x()));
        Point::VectorType tangentStart = rotation * tangent;
        frameRotationStart = std::atan2(tangentStart.y(), tangentStart.x());
    }
    if (m_alignTangentEnd.m_use){
        Point::VectorType axis = m_alignTangentEnd.m_axis;
        Point::Rotation rotation(- std::atan2(axis.y(), axis.x()));
        Point::VectorType tangentEnd = rotation * tangent;
        frameRotationEnd = std::atan2(tangentEnd.y(), tangentEnd.x());
    }

    return frameRotationStart * (1 - t) + frameRotationEnd * t + m_transform->rotation.get();
}

Point::Affine VectorKeyFrame::rigidTransform(float t) const {
    m_spacing->frameChanged(t);
    t = m_spacing->get();

    Point::VectorType pivot = m_pivotCurve != nullptr ? m_pivotCurve->evalArcLength(t) : Point::VectorType::Zero();
    pivot = pivot.hasNaN() ? Point::VectorType(0, 0) : pivot;

    float angleRotation = getFrameRotation(t);
    m_transform->frameChanged(t);

    Point::VectorType translFromPivot = m_transform->translation.get();
    Point::Translation translation(pivot + translFromPivot);

    Point::Rotation rotation(angleRotation); 
    Point::VectorType scaling(m_transform->scaling.get());
    Point::Translation toPivot(pivot);

    Point::VectorType center = getCenterOfGravity(REF_POS) * (1 - t) - getCenterOfGravity(TARGET_POS) * t;
    center = center.hasNaN() ? Point::VectorType(0, 0) : center;
    Point::Translation toCenter(center);


    Point::Affine affine(toCenter);
    affine.scale(scaling);
    affine *= toCenter.inverse();


    Point::Affine ret(toPivot * rotation * toPivot.inverse() * translation * affine);

    return ret;
}

void VectorKeyFrame::resetRigidDeformation() {
    m_pivot->removeKeys("Pivot");
    m_pivot->set(Point::VectorType::Zero());
    m_pivot->addKey("Pivot", 0.0);
    m_pivot->addKey("Pivot", 1.0);
    m_pivot->setInterpolation("Pivot", Curve::LINEAR_INTERP);
    m_pivot->resetTangent();

    m_transform->rotation.removeKeys("Rotation");
    m_transform->rotation.set(0.0);
    m_transform->rotation.addKey("Rotation", 0.0);
    m_transform->rotation.addKey("Rotation", 1.0);
    m_transform->rotation.setInterpolation("Rotation", Curve::LINEAR_INTERP);
    m_transform->rotation.resetTangent();

    m_transform->translation.removeKeys("Translation");
    m_transform->translation.set(Point::VectorType::Zero());
    m_transform->translation.addKey("Translation", 0.0);
    m_transform->translation.addKey("Translation", 1.0);
    m_transform->translation.setInterpolation("Translation", Curve::LINEAR_INTERP);
    m_transform->translation.resetTangent();

    m_transform->scaling.removeKeys("Scaling");
    m_transform->scaling.set(Point::VectorType::Ones());
    m_transform->scaling.addKey("Scaling", 0.0);
    m_transform->scaling.addKey("Scaling", 1.0);
    m_transform->scaling.setInterpolation("Scaling", Curve::LINEAR_INTERP);
    m_transform->scaling.resetTangent();

    m_spacing->setInterpolation("Spacing", Curve::MONOTONIC_CUBIC_INTERP);
    m_spacing->removeKeys("Spacing");
    for (int i = 0; i < 2; ++i) {
        double val = i / 1.0;
        m_spacing->set(val);
        m_spacing->addKey("Spacing", val);
    }
}

void VectorKeyFrame::updateTransforms(Point::VectorType pivotTranslation0, Point::VectorType pivotTranslation1){
    m_transform->translation.frameChanged(0);
    m_transform->translation.set(m_transform->translation.get() - pivotTranslation0);
    m_transform->translation.addKey("Translation", 0.0);

    m_transform->translation.frameChanged(1);
    m_transform->translation.set(m_transform->translation.get() - pivotTranslation1);
    m_transform->translation.addKey("Translation", 1.0);

    makeInbetweensDirty();
    for (Group * group : groups(POST).values()){
        if (group->lattice() != nullptr)
            group->setGridDirty();
    }
}

void VectorKeyFrame::extractPivotTranslation(){
    if (m_pivotTranslationExtracted) return;
    int frame = m_layer->getVectorKeyFramePosition(this);

    // last KeyFrame
    if (this == std::prev(m_layer->keysEnd(), 1).value()){
        VectorKeyFrame * previousKey = std::prev(m_layer->keysEnd(), 2).value();
        KeyframedVector * previousTranslation = previousKey->translation();
        previousTranslation->frameChanged(1);
        m_layer->addPointToPivotCurve(frame, previousKey->getCenterOfGravity(TARGET_POS) + previousTranslation->get() + previousKey->getPivotCurve()->eval(1));
        m_pivotCurve = m_layer->getPivotCurves()->getBezier(m_layer->getFrameTValue(frame));
    }


	// invert translation from REF_POS to TARGET_POS
    else{
        Point::VectorType center = getCenterOfGravity(REF_POS);
        Point::VectorType centerTarget = getCenterOfGravity(TARGET_POS);
        
        Point::Affine T(Point::Translation(centerTarget - center));
        Point::Affine Tinv = T.inverse();

        for (Group * group : groups(POST).values()){
            if (group->lattice() != nullptr)
                group->lattice()->applyTransform(Tinv, TARGET_POS, TARGET_POS);
        }


        m_layer->addPointToPivotCurve(frame, center.hasNaN() ? Point::VectorType::Zero() : center);
        m_pivotCurve = m_layer->getPivotCurves()->getBezier(m_layer->getFrameTValue(frame));
        m_pivotCurve->setP3(centerTarget);

        // invert REF_POS translation
        Point::Affine A(Point::Translation(getCenterOfGravity(REF_POS)));
        Point::Affine inverse = A.inverse();
        for (StrokePtr stroke : strokes().values()){
            stroke->transform(inverse);
        }
        for (Group * group : groups(POST).values()){
            if (group->lattice() != nullptr){
                group->lattice()->applyTransform(inverse, REF_POS, REF_POS);
                group->lattice()->setToRestTransform(A);
                group->lattice()->applyTransform(inverse, TARGET_POS, TARGET_POS);
                group->update();
            }
        }
        
    }

	m_pivotTranslationExtracted = true;
}

void VectorKeyFrame::insertPivotTranslation(){
    int frame = m_layer->getVectorKeyFramePosition(this);

    if (this == std::prev(m_layer->keysEnd(), 1).value()){
        m_layer->deletePointFromPivotCurve(frame);
    }

    else{
        m_transform->translation.frameChanged(0);
        Point::Affine T0(Point::Translation(m_transform->translation.get() + m_pivotCurve->evalArcLength(0)));
        m_transform->translation.frameChanged(1);
        Point::Affine T1(Point::Translation(m_transform->translation.get() + m_pivotCurve->evalArcLength(1)));
        

        for (Group * group : groups(POST).values()){
            if (group->lattice() != nullptr){
                group->lattice()->applyTransform(T0, REF_POS, REF_POS);
                group->lattice()->applyTransform(T1, TARGET_POS, TARGET_POS);
            }
        }
        for (StrokePtr stroke : strokes().values()){
            stroke->transform(T0);
        }
        
    }

    m_pivotCurve = nullptr;
	updateTransforms();
	m_pivotTranslationExtracted = false;
}

void VectorKeyFrame::extractPivotRotation(float startAngle, float endAngle){
    int currentFrame = m_layer->getVectorKeyFramePosition(this);
    int nextFrame = m_layer->getNextKeyFramePosition(currentFrame);

    m_transform->frameChanged(0.0);
	Point::Translation toStart(- m_transform->translation.get());
	Point::Affine RStart(toStart * Point::Rotation(startAngle) * toStart.inverse());
	Point::Affine RStartInv = RStart.inverse();

    m_transform->frameChanged(1.0);
	Point::Translation toEnd(- m_transform->translation.get());
	Point::Affine REnd(toEnd * Point::Rotation(endAngle) * toEnd.inverse());
	Point::Affine REndInv = REnd.inverse();

    for (StrokePtr stroke : strokes().values()){
        stroke->transform(RStartInv);
    }
	for (Group * group : groups(POST).values()){
		if (group->lattice() != nullptr){
			group->lattice()->applyTransform(RStartInv, REF_POS, REF_POS);
            Point::Affine toRest = group->lattice()->getToRestTransform();     
            toRest = toRest * RStart;
            group->lattice()->setToRestTransform(toRest);
			group->lattice()->applyTransform(REndInv, TARGET_POS, TARGET_POS);
            group->update();
		}
	}

	m_transform->rotation.set(startAngle);
	m_transform->rotation.addKey("Rotation", 0);

	m_transform->rotation.set(endAngle);
	m_transform->rotation.addKey("Rotation", 1);

	updateTransforms();
	m_pivotRotationExtracted = true;
}

void VectorKeyFrame::insertPivotRotation(){
    int currentFrame = m_layer->getVectorKeyFramePosition(this);
    int nextFrame = m_layer->getNextKeyFramePosition(currentFrame);

    m_transform->frameChanged(0.0);
	Point::Translation toCenterStart(m_layer->getPivotPosition(currentFrame) - m_transform->translation.get());
	Point::Affine RStart(toCenterStart * Point::Rotation(m_transform->rotation.get()) * toCenterStart.inverse());

    m_transform->frameChanged(1.0);
	Point::Translation toCenterEnd(m_layer->getPivotPosition(nextFrame) - m_transform->translation.get());
	Point::Affine REnd(toCenterEnd * Point::Rotation(m_transform->rotation.get()) * toCenterEnd.inverse());

	for (Group * group : groups(POST).values()){
		if (group->lattice() != nullptr){
			group->lattice()->applyTransform(RStart, REF_POS, REF_POS);
			group->lattice()->applyTransform(REnd, TARGET_POS, TARGET_POS);
		}
	}

    m_transform->rotation.set(0);
    m_transform->rotation.addKey("Rotation", 0);
    m_transform->rotation.addKey("Rotation", 1);
    
    updateTransforms();
    m_pivotRotationExtracted = false;
}

Point::VectorType VectorKeyFrame::getCenterOfGravity(PosTypeIndex type) const {
    Point::VectorType center(0, 0);
    int nb = 0;
    for (Group * group : groups(POST).values()){
        Lattice * lattice = group->lattice();
        if (lattice == nullptr) continue;
        Point::VectorType groupCenter = lattice->centerOfGravity(type);
        if (groupCenter.hasNaN()) continue;
        center += groupCenter;
        nb++;
    }
    return nb == 0 ? Point::VectorType(0, 0) : center / nb;
}


float VectorKeyFrame::optimalRotationAngle(Point::VectorType centerSrc, PosTypeIndex source, Point::VectorType centerTarget, PosTypeIndex target){
    // Eigen::Matrix2d PiPi, QiPi;
    // PiPi.setZero();
    // QiPi.setZero();
    // for (Group * group : groups(POST).values()){
    //     for (Corner * corner : group->lattice()->corners()){
    //         Point::VectorType pi = corner->coord(source) - centerSrc;
    //         Point::VectorType qi = corner->coord(target) - centerTarget;
    //         PiPi += (pi * pi.transpose());
    //         QiPi += (qi * pi.transpose());
    //     }
    // }
    // Eigen::Matrix2d R = QiPi * PiPi.inverse();
    // Point::VectorType p = R * Point::VectorType(1, 0);
    // return atan2(p.y(), p.x());   


    double a = 0.0;
    double b = 0.0;
    for (Group * group : groups(POST).values()){
        for (Corner * corner : group->lattice()->corners()){
            Point::VectorType pi = corner->coord(source) - centerSrc;
            Point::VectorType qi = corner->coord(target) - centerTarget;
            a += qi.dot(pi);
            b += qi.dot(Point::VectorType(-pi.y(), pi.x()));
        }
    }
    double mu = sqrt(a * a + b * b);
    if (mu < 1e-3) mu = 1e-3;
    double r1 = a / mu;
    double r2 = -b / mu;
    Eigen::Matrix2d R;
    R << r1, r2, -r2, r1;
    Point::VectorType p = R * Point::VectorType(1, 0);
    return atan2(p.y(), p.x());
    
}

unsigned int VectorKeyFrame::addTrajectoryConstraint(const std::shared_ptr<Trajectory> &traj) {
    Group *group = traj->group();
    unsigned int idx = pullMaxConstraintIdx();
    m_trajectories.insert(idx, traj);
    group->lattice()->addConstraint(idx);
    group->setGridDirty();
    traj->setConstraintID(idx);
    traj->setHardConstraint(true);
    makeInbetweensDirty();
    return idx;
}

void VectorKeyFrame::removeTrajectoryConstraint(unsigned int id) {
    if (!m_trajectories.contains(id)) {
        qCritical() << "Error in removeTrajectoryConstraint: keyframe does not contain the trajectory ID " << id;
        return;
    }
    const std::shared_ptr<Trajectory> &traj = m_trajectories.value(id); 
    if (traj->nextTrajectory() != nullptr) {
        disconnectTrajectories(traj, traj->nextTrajectory());
    }
    if (traj->prevTrajectory() != nullptr) {
        disconnectTrajectories(traj, traj->prevTrajectory());
    }
    Group *group = traj->group();
    m_trajectories.remove(traj->constraintID());
    group->lattice()->removeConstraint(traj->constraintID());
    group->setGridDirty();
    traj->setHardConstraint(false);
    if (traj->nextTrajectory() != nullptr)
    makeInbetweensDirty();
}

void VectorKeyFrame::connectTrajectories(const std::shared_ptr<Trajectory> &trajA, std::shared_ptr<Trajectory>trajB, bool connectWithNext) {
    if (trajA->keyframe() != this) {
        qCritical() << "connectTrajectories: trajA keyframe is invalid";
        return;
    }

    VectorKeyFrame *nextKey = nextKeyframe();
    VectorKeyFrame *prevKey = prevKeyframe();

    if (trajB->keyframe() != nextKey && trajB->keyframe() != prevKey) {
        Layer *layer = nextKey->parentLayer();
        return;
    }

    if (!m_trajectories.contains(trajA->constraintID())) {
        qCritical() << "connectTrajectories: keyframe A does not contain trajA";
        return;
    }

    if ((connectWithNext && !nextKey->trajectories().contains(trajB->constraintID())) || (!connectWithNext && !prevKey->trajectories().contains(trajB->constraintID()))) {
        qCritical() << "connectTrajectories: keyframe B does not contain trajB";
        return;
    }

    // TODO disconnect if current value is not nullptr
    if (trajA->nextTrajectory() != nullptr && connectWithNext) {
        qWarning() << "trajA was already connected!";
        disconnectTrajectories(trajA, trajA->nextTrajectory());
    } else if (trajA->prevTrajectory() != nullptr && !connectWithNext) {
        qWarning() << "trajA was already connected!";
        disconnectTrajectories(trajA, trajA->prevTrajectory());
    }

    if (connectWithNext) {
        if (trajA->nextTrajectory() != nullptr) disconnectTrajectories(trajA, trajA->nextTrajectory());
        trajA->setNextTrajectory(trajB);
        trajB->setPrevTrajectory(trajA);
    } else {
        if (trajA->prevTrajectory() != nullptr) disconnectTrajectories(trajA, trajA->prevTrajectory());
        trajA->setPrevTrajectory(trajB);
        trajB->setNextTrajectory(trajA);
    }
}

void VectorKeyFrame::disconnectTrajectories(const std::shared_ptr<Trajectory> &trajA, std::shared_ptr<Trajectory> trajB) {
    if (trajA->keyframe() != this) {
        qCritical() << "disconnectTrajectories: trajA keyframe is invalid";
        return;
    }

    VectorKeyFrame *nextKey = nextKeyframe();
    VectorKeyFrame *prevKey = prevKeyframe();

    if (trajB->keyframe() != nextKey && trajB->keyframe() != prevKey) {
        Layer *layer = nextKey->parentLayer();
        return;
    }

    if (!m_trajectories.contains(trajA->constraintID())) {
        qCritical() << "connectTrajectories: keyframe A does not contain trajA";
        return;
    }

    if (trajA->nextTrajectory() == trajB) {
        trajA->setNextTrajectory(nullptr);
        trajB->setPrevTrajectory(nullptr);
    } else if (trajA->prevTrajectory() == trajB) {
        trajA->setPrevTrajectory(nullptr);
        trajB->setNextTrajectory(nullptr);
    } else {
        qWarning() << "disconnectTrajectories: trajA and trajB were not connected";
    }
}

/**
 * Reset all trajectory constraints based on the default (unconstrained) ARAP interpolation.
 * Should be called when the reference or target configuration of a lattice is changed. 
 * If only selected is true, only the trajectories from selected *groups* are reset.
 */
void VectorKeyFrame::resetTrajectories(bool onlySelected) {
    for (const auto &traj : m_trajectories) {
        Group *group = traj->group();
        if (!m_selection.isPostGroupSelected(group->id())) continue;
        group->lattice()->removeConstraint(traj->constraintID());
        group->setGridDirty();
    }

    for (const auto &traj : m_trajectories) {
        if (!m_selection.isPostGroupSelected(traj->group()->id())) continue;
        traj->sampleTrajectory();
    }

    for (const auto &traj : m_trajectories) {
        Group *group = traj->group();
        if (!m_selection.isPostGroupSelected(group->id())) continue;
        group->lattice()->addConstraint(traj->constraintID());
        group->setGridDirty();
    }

    makeInbetweensDirty();
}

/**
 * Toggle constraints of the lattice interpolation for the selected groups 
 */
void VectorKeyFrame::toggleHardConstraint(bool on) {
    for (const auto &traj : m_trajectories) {
        if (!m_selection.isPostGroupSelected(traj->group()->id())) continue;
        if (on && !traj->hardConstraint()) traj->group()->lattice()->addConstraint(traj->constraintID());
        if (!on && traj->hardConstraint()) traj->group()->lattice()->removeConstraint(traj->constraintID());
        traj->setHardConstraint(on);
    }
}

void VectorKeyFrame::updateCurves() {
    // update all curves that should have as many controls points as there are inbetween frames (+ KF as extremeties)
    int inbetweens = m_layer->stride(m_layer->getVectorKeyFramePosition(this)) - 1;
    if (inbetweens < 0) return;
    // spacing
    if (m_spacing->curve()->nbPoints() - 2 != inbetweens) {
        m_spacing->curve()->resample(inbetweens);
    }
    // group spacing
    for (Group *group : m_postGroups) {
        if (group->spacing()->curve()->nbPoints() - 2 != inbetweens) {
            group->spacing()->curve()->resample(inbetweens);
        }
    }
    // trajectories local offset
    for (const auto &traj : m_trajectories) {
        if (traj->localOffset()->curve()->nbPoints() - 2 != inbetweens) {
            traj->localOffset()->curve()->resample(inbetweens);
        }
        if (traj->parentTrajectory() != nullptr) {
            traj->adjustLocalOffsetFromParent();
        }
    }
    // sync partial time with new inbetweens
    m_orderPartials.syncWithFrames(m_layer->stride(m_layer->getVectorKeyFramePosition(this)));
    m_orderPartials.saveState();
}

void VectorKeyFrame::updateBounds(Stroke *stroke) {
    double minX, minY, maxX, maxY;
    if (m_bounds.isNull() || stroke == nullptr) {
        minX = minY = std::numeric_limits<double>::max();
        maxX = maxY = -std::numeric_limits<double>::max();
    } else {
        minX = m_bounds.left();
        minY = m_bounds.top();
        maxX = m_bounds.right();
        maxY = m_bounds.bottom();
    }
    auto update = [&](const Stroke *stroke) {
        for (size_t i = 0; i < stroke->points().size(); i++) {
            Point *point = stroke->points()[i];
            if (point->x() < minX) minX = point->x();
            if (point->x() > maxX) maxX = point->x();
            if (point->y() < minY) minY = point->y();
            if (point->y() > maxY) maxY = point->y();
            m_bounds = QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
        }
    };
    if (stroke != nullptr) { // update current bounds with the new given stroke
        update(stroke);
    } else { // recomputes bounds from scratch
        for (const StrokePtr &stroke : m_strokes) {
            update(stroke.get());          
        }
    }
}

void VectorKeyFrame::transform(QRectF newBoundaries, bool smoothTransform) {
    Q_UNUSED(smoothTransform);
    for (const StrokePtr &s : m_strokes) {
        for (Point *p : s->points()) {
            p->pos().x() = (p->x() - m_bounds.x()) * newBoundaries.width() / m_bounds.width() + newBoundaries.x();
            p->pos().y() = (p->y() - m_bounds.y()) * newBoundaries.height() / m_bounds.height() + newBoundaries.y();
        }
    }
    m_bounds = newBoundaries;
}

int VectorKeyFrame::parentLayerOrder() const {
    auto it = std::find(m_layer->editor()->layers()->indices().begin(), m_layer->editor()->layers()->indices().end(), m_layer->id());
    if (it == m_layer->editor()->layers()->indices().end()) qCritical() << "Error in parentLayerOrder: invalid layer!";
    return it - m_layer->editor()->layers()->indices().begin();
}

VectorKeyFrame *VectorKeyFrame::copy(const QRectF &target) const {
    // if (target.contains(m_bounds)) return copy();
    VectorKeyFrame *result = new VectorKeyFrame(m_layer);
    //
    return result;
}

VectorKeyFrame *VectorKeyFrame::copy() {
    VectorKeyFrame *result = new VectorKeyFrame(m_layer);

    result->m_bounds = m_bounds;
    result->m_currentGroupHue = m_currentGroupHue;

    auto copyGroup = [&](const Group *g, GroupList& groupList) {
        Group *group = new Group(*g);
        group->setParentKeyframe(result);
        groupList.add(group, true);
    };

    // copy strokes
    for (const StrokePtr &s : m_strokes) {
        StrokePtr new_s = std::make_shared<Stroke>(*s); // TODO: option to share stroke for pseudo-instancing?
        result->addStroke(new_s, nullptr, false);
    }
    result->m_visibility = m_visibility;

    // copy groups
    result->m_orderPartials = m_orderPartials;
    result->m_orderPartials.setKeyframe(result);

    for (const Group *g : m_postGroups) {
        copyGroup(g, result->m_postGroups);
    }

    for (const Group *g : m_preGroups) {
        copyGroup(g, result->m_preGroups);
    }

    // copy correspondences
    for (auto it = m_correspondences.constBegin(); it != m_correspondences.constEnd(); ++it) {
        addCorrespondence(it.key(), it.value());
    }

    // TODO traj

    // copy transform and spacing
    delete result->m_transform;
    result->m_transform = new KeyframedTransform(*m_transform);
    delete result->m_spacing;
    result->m_spacing = new KeyframedReal(*m_spacing);

    result->m_alignTangentStart = m_alignTangentStart;
    result->m_alignTangentEnd = m_alignTangentEnd;

    result->m_pivotTranslationExtracted = m_pivotTranslationExtracted;
    result->m_pivotRotationExtracted = m_pivotRotationExtracted;

    result->m_maxStrokeIdx = m_maxStrokeIdx; 

    return result;
}

/**
 * Copy the deformed grid and strokes of srcGroup into the dst keyframe
 * Dst must be the next keyframe!
 * The copied group is by default considered a breakdown of srcGroup since it has the exact same grid topology
 */
void VectorKeyFrame::copyDeformedGroup(VectorKeyFrame *dst, Group *srcGroup, bool makeBreakdown) {
    // TODO if srcGroup already has a correspondence, remove it and continue
    if (dst == nullptr || srcGroup->type() != POST || dst->postGroups().fromId(srcGroup->id()) == nullptr || m_correspondences.contains(srcGroup->id())) {
        qWarning() << "Error in copyGroup: invalid destination keyframe or srcGroup (" << dst << " | " << srcGroup->type() << " | " << dst->postGroups().fromId(srcGroup->id()) << ")";
    }

    int layer = parentLayerOrder();
    int currentFrame = m_layer->getVectorKeyFramePosition(this);
    int frame = m_layer->getVectorKeyFramePosition(dst);
    Editor *editor = m_layer->editor();

    // Copy all deformed stroke segments of srcGroup as new strokes in the dst keyframe
    auto copyStrokes = [&](std::vector<int> &newStrokes) {
        const StrokeIntervals &strokes = srcGroup->strokes(1.0);
        for (auto it = strokes.constBegin(); it != strokes.constEnd(); ++it) {
            Stroke *stroke = m_strokes.value(it.key()).get();
            for (const Interval &interval : it.value()) {
                unsigned int newId = dst->pullMaxStrokeIdx();
                StrokePtr newStroke = std::make_shared<Stroke>(*stroke, newId, interval.from(), interval.to());
                // deform the new stroke with the target configuration of the srcGroup lattice
                for (unsigned int i = interval.from(); i <= interval.to(); ++i) {
                    UVInfo uv = srcGroup->uvs().get(it.key(), i);
                    newStroke->points()[i - interval.from()]->pos() = srcGroup->lattice()->getWarpedPoint(newStroke->points()[i - interval.from()]->pos(), uv.quadKey, uv.uv, TARGET_POS);
                    // Copy strokes visibility
                    dst->visibility()[Utils::cantor(newId, i)] = srcGroup->getParentKeyframe()->visibility().contains(Utils::cantor(stroke->id(), i)) ? srcGroup->getParentKeyframe()->visibility()[Utils::cantor(stroke->id(), i)] : 0.0;
                }
                editor->undoStack()->push(new DrawCommand(editor, layer, frame, newStroke, Group::ERROR_ID, false));
                newStrokes.push_back(newId);

            }
        }
    };

    editor->undoStack()->beginMacro("Copy group");

    // Copy srcGroup into the dst keyframe (as a post group)
    std::vector<int> newStrokesIds;
    copyStrokes(newStrokesIds);
    editor->undoStack()->push(new AddGroupCommand(editor, layer, frame, POST));
    Group *newPostGroup = dst->postGroups().lastGroup();
    for (int id : newStrokesIds) newPostGroup->addStroke(id);

    // Copy srcGroup into the dst keyframe (as a pre group)
    if (makeBreakdown) {
        newStrokesIds.clear();  
        copyStrokes(newStrokesIds);
        editor->undoStack()->push(new AddGroupCommand(editor, layer, frame, PRE));
        Group *newPreGroup = dst->preGroups().lastGroup();
        for (int id : newStrokesIds) newPreGroup->addStroke(id);
        editor->undoStack()->push(new SetCorrespondenceCommand(editor, layer, currentFrame, frame, srcGroup->id(), newPreGroup->id()));
        dst->addIntraCorrespondence(newPreGroup->id(), newPostGroup->id());
    }
    
    // Copy lattice and set the new ref position as the previous target position
    newPostGroup->setColor(srcGroup->color());
    newPostGroup->setGrid(new Lattice(*srcGroup->lattice()));
    newPostGroup->lattice()->setKeyframe(dst);
    for (Corner *c : srcGroup->lattice()->corners()) {
        int key = c->getKey();
        newPostGroup->lattice()->corners()[key]->coord(REF_POS) = newPostGroup->lattice()->corners()[key]->coord(TARGET_POS);
    }

    // Rebake strokes
    newPostGroup->strokes().forEachInterval([&](const Interval &interval, unsigned int strokeID) { 
        editor->grid()->bakeStrokeInGrid(newPostGroup->lattice(), dst->stroke(strokeID), interval.from(), interval.to());
        editor->grid()->bakeStrokeInGrid(srcGroup->lattice(), dst->stroke(strokeID), interval.from(), interval.to(), TARGET_POS, false);
    });

    // Rebake UV
    for (auto it = newPostGroup->strokes().begin(); it != newPostGroup->strokes().end(); ++it) {
        Stroke *stroke = dst->stroke(it.key());
        for (Interval &interval : it.value()) {
            newPostGroup->lattice()->bakeForwardUV(stroke, interval, newPostGroup->uvs());
        }
    }    

    // Dirty flags
    newPostGroup->setGridDirty();
    newPostGroup->lattice()->resetPrecomputedTime();
    newPostGroup->lattice()->setBackwardUVDirty(true);
    srcGroup->lattice()->setBackwardUVDirty(true);
    makeInbetweensDirty();
    dst->makeInbetweensDirty();

    editor->undoStack()->endMacro();
}

void VectorKeyFrame::paste(VectorKeyFrame *other) {
    // for (const Stroke *s : other->m_strokes) {
    //     Stroke *new_s = new Stroke(*s);
    //     addStroke(new_s);
    // }
}

void VectorKeyFrame::paste(VectorKeyFrame *source, const QRectF &target) {
    // VectorKeyFrame *k = source->copy();
    // k->transform(target, true);
    // paste(k);
}

void VectorKeyFrame::initOriginStrokes() {
    for(auto group : m_postGroups) {
        group->initOriginStrokes();
    }
}

void VectorKeyFrame::resetOriginStrokes() {
    for(auto group : m_postGroups) {
        group->resetOriginStrokes();
    }
}

/**
 * Create a breakdown KF from the current inbetween
 * 
 * @param editor 
 * @param newKeyframe The new breakdown keyframe
 * @param nextKeyframe The keyframe following the breakdown
 * @param inbetweenCopy Baseline of the breakdown
 * @param inbetween Inbetween idx
 * @param alpha Interpolation factor of the inbetween
 */
void VectorKeyFrame::createBreakdown(Editor *editor, VectorKeyFrame *newKeyframe, VectorKeyFrame *nextKeyframe, const Inbetween& inbetweenCopy, int inbetween, qreal alpha) {
    if (newKeyframe == nullptr) return;

    std::unordered_map<int, int> groupIdMap;

    // retrieve the baked inbetween strokes
    QHash<int, int> backwardStrokesMapping;
    newKeyframe->strokes() = inbetweenCopy.strokes;
    newKeyframe->m_visibility = m_visibility;
    newKeyframe->m_maxStrokeIdx = m_maxStrokeIdx; 
    int backwardStart = newKeyframe->m_maxStrokeIdx;

    // remap stroke visibility 
    for (auto it = newKeyframe->m_visibility.begin(); it != newKeyframe->m_visibility.end(); ++it) {
        if (it.value() >= 0.0) {
            it.value() = std::clamp(Utils::map(it.value(), alpha, 1.0, 0.0, 1.0), 0.0, 1.0);
        } else {
            it.value() = std::clamp(Utils::map(it.value(), -1.0, -alpha, -1.0, -1e-8), -1.0, -1e-8);
        }
    }

    // add strokes from the next keyframe and create a mapping of their IDs in both KF
    for (auto it = inbetweenCopy.backwardStrokes.constBegin(); it != inbetweenCopy.backwardStrokes.constEnd(); ++it) {
        StrokePtr strokeCopy = std::make_shared<Stroke>(*it.value());
        strokeCopy->resetID(newKeyframe->pullMaxStrokeIdx());
        newKeyframe->addStroke(strokeCopy, nullptr, false);
        backwardStrokesMapping.insert(it.key(), strokeCopy->id());
    }

    // split global rigid transform
    m_spacing->frameChanged(alpha);
    KeyframedTransform *secondHalf = m_transform->split(m_spacing->get());
    delete newKeyframe->m_transform; 
    newKeyframe->m_transform = secondHalf;

    // split post groups and pre groups
    Point::Affine globalRigidTransform = rigidTransform(1.0f);
    for (Group *group : m_postGroups) {
        if (group->size() == 0) continue;

        Group *newGroup = newKeyframe->postGroups().add(true);
        group->makeBreakdown(newKeyframe, nextKeyframe, newGroup, inbetween, alpha, globalRigidTransform * group->rigidTransform(1.0f), backwardStrokesMapping, editor);
        groupIdMap.insert({group->id(), newGroup->id()});

        // add duplicated corresponding pre group
        Group *newPreGroup = newKeyframe->preGroups().add(true);
        // TODO duplicate pre group strokes? (should update backward stroke uvs)
        newPreGroup->strokes() = newGroup->strokes();
        newPreGroup->recomputeBbox();
        newPreGroup->setBreakdown(true);
        // remake correspondences
        if (m_correspondences.contains(group->id())) {
            int nextGroupId = m_correspondences[group->id()];
            newKeyframe->addCorrespondence(newGroup->id(), nextGroupId);
        }
        m_correspondences[group->id()] = newPreGroup->id();
        newKeyframe->addIntraCorrespondence(newPreGroup->id(), newGroup->id());
    }

    // TODO: split new key's pivot spacing

    // remap order partials
    OrderPartial firstPartial = m_orderPartials.lastPartialAt(alpha);
    firstPartial.setKeyframe(newKeyframe);
    firstPartial.setT(0.0);
    newKeyframe->m_orderPartials = Partials<OrderPartial>(newKeyframe, firstPartial);
    OrderPartial &firstP = newKeyframe->m_orderPartials.firstPartial();
    for (std::vector<int> &depth : firstP.groupOrder().order()) {
        for (int i = 0; i < depth.size(); ++i) {
            depth[i] = groupIdMap[depth[i]];
        }
    }
    for (const OrderPartial &partial : m_orderPartials.partials()) {
        if (partial.t() >= alpha) {
            OrderPartial newPartial = partial;
            newPartial.setKeyframe(newKeyframe);
            newPartial.setT(Utils::map(partial.t(), alpha, 1.0, 0.0, 1.0));
            newKeyframe->m_orderPartials.insertPartial(newPartial);
            OrderPartial &p = newKeyframe->m_orderPartials.lastPartialAt(newPartial.t());
            for (auto depth : p.groupOrder().order()) {
                for (int i = 0; i < depth.size(); ++i) {
                    depth[i] = groupIdMap[depth[i]];
                }
            }
        }
    }

    // TODO remap order partial on prev segment

    newKeyframe->updateBounds(nullptr);
    makeInbetweensDirty();
    newKeyframe->makeInbetweensDirty();
}

/**
 * Add a corresponding "PRE" group to the given "POST" group with all the strokes from the next KF that fit into the post group's lattice.
*/
void VectorKeyFrame::toggleCrossFade(Editor *editor, Group *post) {
    int layer = parentLayerOrder();
    VectorKeyFrame *next = nextKeyframe();
    int currentFrame = m_layer->getVectorKeyFramePosition(this);
    int nextFrame = m_layer->getVectorKeyFramePosition(next);
    if (nextFrame == m_layer->getMaxKeyFramePosition() || post->type() != POST || post->nextPreGroup() != nullptr) return;

    editor->undoStack()->beginMacro("Add cross-fade");

    // Select strokes segments in the next KF that overlaps with the deformed grid
    Group *newPreGroup = nullptr;
    StrokeIntervals backwardStrokes;
    for (Group *group : next->postGroups()) {
        for (auto it = group->strokes().constBegin(); it != group->strokes().constEnd(); ++it) {
            // Try to fit stroke segments from the next KF into the lattice
            Stroke* stroke = next->stroke(it.key());
            auto [startIdx, endIdx] = editor->grid()->expandTargetGridToFitStroke(post->lattice(), stroke);
            if (startIdx == -1 || endIdx == -1) continue;

            // If we find a stroke segment that fits, clone it as a new stroke and bake it in the grid
            unsigned int newId = next->pullMaxStrokeIdx();
            StrokePtr copiedStroke = std::make_shared<Stroke>(*stroke, newId, startIdx, endIdx);
            editor->undoStack()->push(new DrawCommand(editor, layer, nextFrame, copiedStroke, Group::ERROR_ID, false));
            editor->grid()->bakeStrokeInGrid(post->lattice(), copiedStroke.get(), 0, copiedStroke->size() - 1, TARGET_POS, false);
            post->lattice()->deleteQuadsPredicate([&](QuadPtr q) { return (q->nbForwardStrokes() == 0 && q->nbBackwardStrokes() == 0 && !q->isPivot()); });
            backwardStrokes[newId].append(Interval(0, copiedStroke->size() - 1));
        }
    }

    // Add the backward strokes in to a new "PRE" group in the next KF and create correspondences
    editor->undoStack()->push(new AddGroupCommand(editor, layer, nextFrame, PRE)); 
    newPreGroup = next->preGroups().lastGroup();
    editor->undoStack()->push(new SetGroupCommand(editor, layer, nextFrame, backwardStrokes, newPreGroup->id(), PRE));
    editor->undoStack()->push(new SetCorrespondenceCommand(editor, layer, currentFrame, nextFrame, post->id(), newPreGroup->id()));
    editor->undoStack()->endMacro();
}

float VectorKeyFrame::getNextGroupHue() { 
    float prev = m_currentGroupHue; 
    m_currentGroupHue = std::fmod(m_currentGroupHue + 0.618033988749895, 1.0); 
    return prev;
}