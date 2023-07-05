/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

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
#include "utils/stopwatch.h"

#include <QtGui>
#include <limits>
#include <iostream>

using namespace std;

static dkBool k_resample("Pen->Resample stroke", true);
static dkFloat k_minSampling("Pen->Min sampling", 1.5, 0.01, 10.0, 0.01);
static dkFloat k_maxSampling("Pen->Max sampling", 5.0, 0.01, 10.0, 0.01);
static dkBool k_hideMainGroup("Options->Drawing->Hide main group", false);
static dkBool k_showPivot("RigidDeform->Show pivot", false);
static dkBool k_alignToTangent("RigidDeform->Align to tangent", false);
static dkBool k_useCrossFade("Options->Drawing->Use Cross Fade", true);

VectorKeyFrame::VectorKeyFrame(Layer *layer) 
    : m_layer(layer),
      m_currentGroupHue(0.0f),
      m_maxStrokeIdx(0),
      m_preGroups(PRE, this),
      m_postGroups(POST, this),
      m_selection(this),
      m_pivot(Point::VectorType::Zero()),
      m_transform(new KeyframedTransform("Transform")),
      m_spacing(new KeyframedFloat("Spacing")),
      m_alignFrameToTangent(false),
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
    m_bounds = QRectF();
    // clear trajectories
    m_trajectories.clear();
    // reset properties
    m_currentGroupHue = 0.0f;
    m_maxStrokeIdx = 0;
    m_maxConstraintIdx = 0;
    // reset all deformations
    resetRigidDeformation();
    // restore default group
    m_postGroups.add(new Group(this, QColor(Qt::black), MAIN));
}

StrokePtr VectorKeyFrame::addStroke(const StrokePtr &stroke, Group *group, bool resample) {
    if (m_strokes.contains(stroke->id())) {
        qCritical() << "Error! This keyframe already has a stroke with the id: " << stroke->id();
        return nullptr;
    }

    // resample stroke
    StrokePtr newStroke;
    if (k_resample && resample) {
        newStroke = stroke->resample(k_maxSampling, k_minSampling); //
    } else {
        newStroke = stroke;
    }

    updateBounds(newStroke.get()); //
    newStroke->computeNormals();
    newStroke->computeOutline();

    m_strokes.insert(newStroke->id(), newStroke); //

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
        stroke->updateBuffer();
    }
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
 * Compute an inbetween frame. 
 * An inbetween frame is made of 2 sets of strokes:
 *     - forward strokes coming from the previous keyframe
 *     - backward strokes coming from the next keyframe (if there is a correspondence)
 * This function takes as input an interpolating factor alpha in [0,1] and fills both sets of strokes and the interpolated grid corners.
 * Note that strokes opacity or thickness is not yet interpolated at this point, instead this is done during the rendering of the inbetween.
*/
void VectorKeyFrame::computeIntermediateStrokes(float alpha, QHash<int, StrokePtr> &intermediateStrokes, QHash<int, StrokePtr> &backwardIntermediateStrokes, QHash<int, std::vector<Point::VectorType>> &corners) const {
    intermediateStrokes.clear();
    Point::Affine rigidTrans = rigidTransform(alpha);
    Point::Affine invRigidTransform = rigidTransform(1.0).inverse();

    // Copy forward strokes
    for (const StrokePtr &stroke : m_strokes) {
        intermediateStrokes.insert(stroke->id(), std::make_shared<Stroke>(*stroke));
    }

    // Compute the interpolated deformation of each group and use it to warp forward and backward strokes
    for (Group *group : m_postGroups) {
        if (group->size() > 0) {
            float spacing = group->spacingAlpha(alpha);
            if (group->lattice() == nullptr) return;
            // Interpolate the lattice if this is not done
            if (group->lattice()->isArapPrecomputeDirty()) {
                group->lattice()->precompute();
            }
            if (group->lattice()->isArapInterpDirty() || spacing != group->lattice()->currentPrecomputedTime()) {
                group->lattice()->interpolateARAP(alpha, spacing, rigidTransform(alpha));
            }
            // Use the interpolated lattice to compute the interpolated forward strokes
            for (auto it = group->strokes().begin(); it != group->strokes().end(); ++it) {
                const StrokePtr &stroke = intermediateStrokes[it.key()]; 
                const Intervals &intervals = group->strokes().value(it.key());
                for (const Interval &interval : intervals) {
                    for (unsigned int i = interval.from(); i <= interval.to(); i++) {
                        UVInfo uv = group->uvs().get(it.key(), i);
                        stroke->points()[i]->pos() = group->lattice()->getWarpedPoint(stroke->points()[i]->pos(), uv.quadKey, uv.uv, INTERP_POS);
                    }
                }
            }
            // Save the lattice interpolated corners (mainly for debugging)
            if (corners.contains(group->id())) corners[group->id()].clear();
            std::vector<Point::VectorType> &cornersPoint = corners[group->id()];
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
            for (auto it = next->strokes().begin(); it != next->strokes().end(); ++it) {
                Stroke *stroke = next->stroke(it.key());
                StrokePtr newStroke = std::make_shared<Stroke>(*stroke);
                backwardIntermediateStrokes.insert(newStroke->id(), newStroke);
                for (auto itIntervals = it.value().begin(); itIntervals != it.value().end(); ++itIntervals) {
                    float spacing = group->spacingAlpha(alpha);
                    if (group->lattice() == nullptr) return;
                    // Interpolate the lattice if this is not done
                    if (group->lattice()->isArapPrecomputeDirty()) group->lattice()->precompute();
                    if (group->lattice()->isArapInterpDirty() || spacing != group->lattice()->currentPrecomputedTime()) group->lattice()->interpolateARAP(alpha, spacing, rigidTransform(alpha));
                    // Use the interpolated lattice to compute the interpolated backward strokes. If the backward UVs are dirty, rebake them
                    if (group->lattice()->backwardUVDirty()) group->lattice()->bakeBackwardUV(newStroke.get(), (*itIntervals), invRigidTransform, group->backwardUVs());
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
    for (int i = 0; i < stride; ++i) {
        m_inbetweens.emplace_back(Inbetween());
        m_inbetweens.makeDirty();
    }
    qDebug() << "(Re)Initializing inbetweens";
}

void VectorKeyFrame::bakeAllInbetweens(Editor *editor) {
    // m_inbetweens.clear();

    // int currentFrame = editor->playback()->currentFrame();
    // int lastFrame = m_layer->getLastKeyFramePosition(currentFrame);
    // int nextFrame = m_layer->getNextKeyFramePosition(currentFrame);
    // int nbInbetweens = nextFrame - lastFrame;

    // float alphaLinear = 0.0;
    // for (int i = 0; i < nbInbetweens; ++i) {
    //     alphaLinear = editor->alpha(lastFrame + i + 1);
    //     m_inbetweens.emplace_back(QHash<int, StrokePtr>());
    //     computeIntermediateStrokes(alphaLinear, m_inbetweens.back());
    //     m_inbetweens.makeClean(i);
    // }
}

/**
 * Compute and cache the inbetween frame 
*/
void VectorKeyFrame::bakeInbetween(Editor *editor, int frame, int inbetween, int stride) {
    if (inbetween > m_inbetweens.size()) {
        qCritical() << "Invalid inbetween vector size! (" << inbetween << " vs " << m_inbetweens.size() << ")";
        return;
    }

    int idx = inbetween - 1;
    if (m_inbetweens.isClean(idx)) return;

    if (stride <= 0 || inbetween > stride || inbetween <= 0) return;

    float alphaLinear = editor->alpha(frame + inbetween);
    if (alphaLinear == 0.0f && inbetween == stride) alphaLinear = 1.0f;

    m_inbetweens[idx].destroyBuffers();
    m_inbetweens[idx].strokes.clear();
    m_inbetweens[idx].backwardStrokes.clear();
    m_inbetweens[idx].corners.clear();
    computeIntermediateStrokes(alphaLinear, m_inbetweens[idx].strokes, m_inbetweens[idx].backwardStrokes, m_inbetweens[idx].corners);
    m_inbetweens.makeClean(idx);

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

void VectorKeyFrame::paintImage(QPainter &painter, float alpha, int inbetween, const QColor &color, qreal tintFactor, bool useGroupColor) {
    static QPen pen(QBrush(color, Qt::SolidPattern), 0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    painter.save();

    const QHash<int, StrokePtr> &forwardStrokes = inbetween > 0 ? m_inbetweens[inbetween - 1].strokes : m_strokes;

    auto drawStrokeIntervals = [&](const QHash<int, StrokePtr> &strokes, const StrokeIntervals &strokeIntervals, const QColor &groupColor, float scaling, bool overshoot) {
        for (auto it = strokeIntervals.begin(); it != strokeIntervals.end(); ++it) {
            if (!strokes.contains(it.key())) {
                qCritical() << "strokes size=" << strokes.size();
            }
            StrokePtr stroke = strokes.value(it.key());
            if (useGroupColor)          pen.setColor(groupColor);
            else if (tintFactor > 0)    pen.setColor(tintColor(stroke, tintFactor, color));
            else                        pen.setColor(stroke->color());
            for (auto itIntervals = it.value().begin(); itIntervals != it.value().end(); ++itIntervals) {
                const Interval &interval = *itIntervals;
                stroke->draw(painter, pen, interval.from(), interval.to(), scaling, overshoot && interval.canOvershoot());
            }
        }
    };

    // paint post groups (with cross-fade or not)
    Group *next = nullptr;
    bool drawNext = false;
    float widthScalingForward, widthScalingBackward;
    for (Group *group : m_postGroups) {
        next = group->nextPreGroup();
        drawNext = next != nullptr && k_useCrossFade;
        widthScalingForward =  drawNext ? group->crossFadeValue(group->spacingAlpha(alpha), true)  : 1.0;
        widthScalingBackward = drawNext ? group->crossFadeValue(group->spacingAlpha(alpha), false) : 1.0; 
        if (group->disappear()) widthScalingForward = std::max(1.0 - group->spacingAlpha(alpha), 0.0);

        drawStrokeIntervals(forwardStrokes, group->strokes(), group->color(), widthScalingForward, inbetween == 0);

        if (drawNext && inbetween > 0) {
            drawStrokeIntervals(m_inbetweens[inbetween - 1].backwardStrokes, next->strokes(), group->color(), widthScalingBackward, false);
        }
    }

    // paint only the selected pre groups
    painter.setOpacity(0.75);
    useGroupColor = true;
    for (Group *group : m_selection.selectedPreGroups()) {
        drawStrokeIntervals(m_strokes, group->strokes(), group->color(), 1.0f, false);
    }

    painter.restore();
    // TODO draw pivot?
}

void VectorKeyFrame::paintImageGL(QOpenGLShaderProgram *program, QOpenGLFunctions *functions, float alpha, qreal opacityAlpha, int inbetween, const QColor &color, qreal tintFactor, bool useGroupColor) {
    for (Group *group : m_postGroups) {
        paintGroupGL(program, functions, alpha, opacityAlpha, group, inbetween, color, tintFactor, 1.0, useGroupColor);
    } 
}

void VectorKeyFrame::paintImageGL(QOpenGLShaderProgram *program, QOpenGLFunctions *functions, qreal opacityAlpha, const QColor &color, qreal tintFactor, bool useGroupColor, GroupType type) {
    const GroupList &groups = type == POST ? m_postGroups : m_preGroups;
    for (Group *group : groups) {
        paintGroupGL(program, functions, opacityAlpha, group, color, tintFactor, 1.0, useGroupColor);
    }
}

void VectorKeyFrame::paintGroup(QPainter &painter, Group *group, int inbetween, const QColor &color, qreal tintFactor, bool useGroupColor) {
    static QPen pen(QBrush(color, Qt::SolidPattern), 0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    
    painter.save();

    const QHash<int, StrokePtr> &strokes = inbetween > 0 ? m_inbetweens[inbetween - 1].strokes : m_strokes;
    const StrokeIntervals &strokeIntervals = group->strokes();

    for (auto it = strokeIntervals.begin(); it != strokeIntervals.end(); ++it) {
        const StrokePtr &stroke = strokes.value(it.key());
        if (tintFactor > 0) pen.setColor(tintColor(stroke, tintFactor, color));
        else pen.setColor(stroke->color());
        for (auto itIntervals = it.value().begin(); itIntervals != it.value().end(); ++itIntervals) {
            const Interval &interval = *itIntervals;
            stroke->draw(painter, pen, interval.from(), interval.to(), 1.0, inbetween == 0  && interval.canOvershoot());
        }
    }
    //TODO cross fade here

    painter.restore();
}

void VectorKeyFrame::paintGroupGL(QOpenGLShaderProgram *program, QOpenGLFunctions *functions, float alpha, double opacityAlpha, Group *group, int inbetween, const QColor &color, double tintFactor, double strokeWeightFactor, bool useGroupColor, bool crossFade) {
    const QHash<int, StrokePtr> &strokes = inbetween > 0 ? m_inbetweens[inbetween - 1].strokes : m_strokes;
    const StrokeIntervals &strokeIntervals = group->strokes();

    Group *next = group->nextPreGroup();
    bool drawNext = next != nullptr && crossFade && k_useCrossFade;
    float widthScalingForward =  drawNext ? group->crossFadeValue(group->spacingAlpha(alpha), true)  : 1.0;
    float widthScalingBackward = drawNext ? group->crossFadeValue(group->spacingAlpha(alpha), false) : 1.0; 
    if (group->disappear()) widthScalingForward = std::max(1.0 - group->spacingAlpha(alpha), 0.0);
    if (drawNext && group->size() == 0) widthScalingBackward = std::max(group->spacingAlpha(alpha), 0.0);
    
    // draw forward strokes
    for (auto it = strokeIntervals.begin(); it != strokeIntervals.end(); ++it) {
        const StrokePtr &stroke = strokes.value(it.key());
        if (!stroke->buffersCreated()) stroke->createBuffers(program);
        QColor color_alpha;
        if (useGroupColor)          color_alpha = group->color();
        else if (tintFactor > 0.0)  color_alpha = tintColor(stroke, tintFactor, color);
        else                        color_alpha = stroke->color();
        color_alpha.setAlphaF(opacityAlpha * widthScalingForward);
        program->setUniformValue("stroke_color", color_alpha);
        program->setUniformValue("stroke_weight", (float)stroke->strokeWidth() * widthScalingForward * (float)strokeWeightFactor);
        for (const Interval &interval : it.value()) {
            int cap[2] = {(int)interval.from(), (int)interval.to()}; // TODO: do this more properly
            if (inbetween == 0 && interval.canOvershoot() && interval.to() < stroke->size() - 1) cap[1] += 1;
            program->setUniformValueArray("cap_idx", cap, 2);
            stroke->render(GL_LINE_STRIP_ADJACENCY, functions, interval, inbetween == 0);
        }
    }

    // draw backward strokes (if cross-fade is enabled)
    // TODO factorize with above
    if (drawNext && inbetween > 0) {
        for (auto it = next->strokes().begin(); it != next->strokes().end(); ++it) {
            const StrokePtr &stroke = m_inbetweens[inbetween - 1].backwardStrokes.value(it.key());
            if (!stroke->buffersCreated()) stroke->createBuffers(program);
            QColor color_alpha;
            if (useGroupColor)          color_alpha = group->color();
            else if (tintFactor > 0.0)  color_alpha = tintColor(stroke, tintFactor, color);
            else                        color_alpha = stroke->color();
            color_alpha.setAlphaF(opacityAlpha * widthScalingBackward);
            program->setUniformValue("stroke_color", color_alpha);
            program->setUniformValue("stroke_weight", (float)stroke->strokeWidth() * widthScalingBackward * (float)strokeWeightFactor);
            for (const Interval &interval : it.value()) {
                int cap[2] = {(int)interval.from(), (int)interval.to()}; // TODO: do this more properly
                program->setUniformValueArray("cap_idx", cap, 2);
                stroke->render(GL_LINE_STRIP_ADJACENCY, functions, interval, false);
            }
        }
    }  
}

void VectorKeyFrame::paintGroupGL(QOpenGLShaderProgram *program, QOpenGLFunctions *functions, double opacityAlpha, Group *group,const QColor &color, double tintFactor, double strokeWeightFactor,  bool useGroupColor) {
    const StrokeIntervals &strokeIntervals = group->strokes();
    for (auto it = strokeIntervals.begin(); it != strokeIntervals.end(); ++it) {
        const StrokePtr &stroke = m_strokes.value(it.key());
        if (!stroke->buffersCreated()) stroke->createBuffers(program);
        QColor color_alpha;
        if (useGroupColor)          color_alpha = group->color();
        else if (tintFactor > 0.0)  color_alpha = tintColor(stroke, tintFactor, color);
        else                        color_alpha = stroke->color();
        color_alpha.setAlphaF(opacityAlpha);
        program->setUniformValue("stroke_color", color_alpha);
        program->setUniformValue("stroke_weight", (float)stroke->strokeWidth() * (float)strokeWeightFactor);
        for (const Interval &interval : it.value()) {
            int cap[2] = {(int)interval.from(), (int)interval.to()};
            if (interval.canOvershoot() && interval.to() < stroke->size() - 1) cap[1] += 1;
            program->setUniformValueArray("cap_idx", cap, 2);
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
            StrokePtr s = std::make_shared<Stroke>(strokeId, c, thickness, false);
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
            }

            Group *group = new Group(this, POST);
            group->load(groupNode);
            group->update();
            m_postGroups.add(group);
            groupNode = groupNode.nextSibling();
        }
    } else { // backward compatibility: load strokes directly into a single group        
        for (const StrokePtr &stroke : m_strokes) {
            defaultGroup()->addStroke(stroke->id());
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
        editor->grid()->constructGrid(defaultGroup(), editor->view());
        defaultGroup()->update();
    }

    // Restore grid-stroke correspondence
    for (Group *group : m_postGroups) {
        for (auto it = group->strokes().begin(); it != group->strokes().end(); ++it) {
            for (Interval &interval : it.value()) {
                const StrokePtr &stroke = m_strokes[it.key()];
                editor->grid()->bakeStrokeInGrid(group->lattice(), stroke.get(), interval.from(), interval.to());
                group->lattice()->bakeForwardUV(stroke.get(), interval, group->uvs());
            }
        }
    }
    
    // load global rigid trajectory
    QDomElement pivotElt = strokesElt.nextSiblingElement("pivot");
    m_pivot = Point::VectorType(pivotElt.attribute("px").toFloat(), pivotElt.attribute("py").toFloat());
    QDomElement translationElt = strokesElt.nextSiblingElement("translation");
    if (!translationElt.isNull()) m_transform->translation.load(translationElt);
    QDomElement rotationElt = strokesElt.nextSiblingElement("rotation");
    if (!rotationElt.isNull()) m_transform->rotation.load(rotationElt);
    QDomElement spacingElt = strokesElt.nextSiblingElement("spacing");
    if (!spacingElt.isNull()) m_spacing->load(spacingElt);
    QDomElement rigidTransformElt = strokesElt.nextSiblingElement("rigidTransform");
    if (!rigidTransformElt.isNull()) m_transform->load(rigidTransformElt);

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

    // load correspondences
    QDomElement correspondences = strokesElt.nextSiblingElement("corresp");
    if (correspondences.isNull()) qDebug() << "Loading: could not find correspondences";
    int size = correspondences.attribute("size").toInt();
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
    size = intraCorrespondences.attribute("size").toInt();
    QString stringIntra = intraCorrespondences.text();
    QTextStream posIntra(&stringIntra);
    for (int i = 0; i < size; ++i) {
        posIntra >> groupA >> groupB;
        addIntraCorrespondence(groupA, groupB);
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
            traj->group()->lattice()->setArapDirty();
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

    // save global rigid trajectory
    QDomElement pivotElt = doc.createElement("pivot");
    pivotElt.setAttribute("px", m_pivot.x());
    pivotElt.setAttribute("py", m_pivot.y());
    keyElt.appendChild(pivotElt);
    QDomElement rigidTransformElt = doc.createElement("rigidTransform");
    m_transform->save(doc, rigidTransformElt);
    keyElt.appendChild(rigidTransformElt);
    QDomElement spacingElt = doc.createElement("spacing");
    m_spacing->save(doc, spacingElt);
    keyElt.appendChild(spacingElt);

    // save correspondences
    QDomElement correspondencesElt = doc.createElement("corresp");
    correspondencesElt.setAttribute("size", (int)m_correspondences.size());
    QString string;
    QTextStream startPos(&string);
    for (auto it = m_correspondences.constBegin(); it != m_correspondences.constEnd(); ++it) {
        startPos << it.key() << " " << it.value() << " ";
    }
    QDomText txt = doc.createTextNode(string);
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

    root.appendChild(keyElt);
    return true;
}

Point::Affine VectorKeyFrame::rigidTransform(float t) const {
    m_spacing->frameChanged(t);
    t = m_spacing->get();

    m_transform->frameChanged(t);

    Point::VectorType tangent = m_transform->translation.getDerivative();
    float frameRotation = m_alignFrameToTangent ? std::atan2(tangent.y(), tangent.x()) : 0.0f;

    Point::Translation translation(m_transform->translation.get());
    Point::Rotation rotation(m_transform->rotation.get() + frameRotation); 
    Point::Translation toPivot(-m_pivot);

    return Point::Affine(translation * toPivot.inverse() * rotation * toPivot);
}

void VectorKeyFrame::resetRigidDeformation() {
    m_pivot = Point::VectorType::Zero();

    m_transform->rotation.removeKeys("Rotation");
    m_transform->rotation.set(0.0);
    m_transform->rotation.addKey("Rotation", 0.0);
    m_transform->rotation.addKey("Rotation", 1.0);
    m_transform->rotation.setInterpolation("Rotation", Curve::HERMITE_INTERP);
    m_transform->rotation.resetTangent();

    m_transform->translation.removeKeys("Translation");
    m_transform->translation.set(Point::VectorType::Zero());
    m_transform->translation.addKey("Translation", 0.0);
    m_transform->translation.addKey("Translation", 1.0);
    m_transform->translation.setInterpolation("Translation", Curve::HERMITE_INTERP);
    m_transform->translation.resetTangent();

    m_transform->scaling.removeKeys("Scaling");
    m_transform->scaling.set(Point::VectorType::Ones());
    m_transform->scaling.addKey("Scaling", 0.0);
    m_transform->scaling.addKey("Scaling", 1.0);
    m_transform->scaling.setInterpolation("Scaling", Curve::HERMITE_INTERP);
    m_transform->scaling.resetTangent();

    m_spacing->setInterpolation("Spacing", Curve::MONOTONIC_CUBIC_INTERP);
    m_spacing->removeKeys("Spacing");
    for (int i = 0; i < 2; ++i) {
        double val = i / 1.0;
        m_spacing->set(val);
        m_spacing->addKey("Spacing", val);
    }
}

unsigned int VectorKeyFrame::addTrajectoryConstraint(const std::shared_ptr<Trajectory> &traj) {
    Group *group = traj->group();
    unsigned int idx = pullMaxConstraintIdx();
    m_trajectories.insert(idx, traj);
    group->lattice()->addConstraint(idx);
    group->lattice()->setArapDirty();
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
    group->lattice()->setArapDirty();
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
 */
void VectorKeyFrame::resetTrajectories() {
    for (const auto &traj : m_trajectories) {
        Group *group = traj->group();
        group->lattice()->removeConstraint(traj->constraintID());
        group->lattice()->setArapDirty();
    }

    for (const auto &traj : m_trajectories) {
        traj->sampleTrajectory();
    }

    for (const auto &traj : m_trajectories) {
        Group *group = traj->group();
        group->lattice()->addConstraint(traj->constraintID());
        group->lattice()->setArapDirty();
    }

    makeInbetweensDirty();
}

void VectorKeyFrame::toggleHardConstraint(bool on) {
    for (const auto &traj : m_trajectories) {
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

int VectorKeyFrame::parentLayerId() const {
    auto it = std::find(m_layer->editor()->layers()->indices().begin(), m_layer->editor()->layers()->indices().end(), m_layer->id());
    if (it == m_layer->editor()->layers()->indices().end()) qCritical() << "Error in parentLayerId: invalid layer!";
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
        StrokePtr new_s = std::make_shared<Stroke>(*s); //
        result->addStroke(new_s, nullptr, false);
    }

    // copy groups
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
    result->m_spacing = new KeyframedFloat(*m_spacing);

    result->m_maxStrokeIdx = m_maxStrokeIdx; 

    return result;
}

/**
 * Copy the deformed grid and strokes of srcGroup into the dst keyframe
 * Dst must be the next keyframe!
 * The copied group is by default considered a breakdown of srcGroup since it has the exact same grid topology
 */
void VectorKeyFrame::copyDeformedGroup(VectorKeyFrame *dst, Group *srcGroup) {
    // TODO if srcGroup already has a correspondence, remove it and continue
    if (dst == nullptr || srcGroup->type() != POST || dst->postGroups().fromId(srcGroup->id()) == nullptr || m_correspondences.contains(srcGroup->id())) {
        qWarning() << "Error in copyGroup: invalid destination keyframe or srcGroup";
    }

    int layer = parentLayerId();
    int currentFrame = m_layer->getVectorKeyFramePosition(this);
    int frame = m_layer->getVectorKeyFramePosition(dst);
    Editor *editor = m_layer->editor();

    // Copy all deformed stroke segments of srcGroup as new strokes in the dst keyframe
    auto copyStrokes = [&](std::vector<int> &newStrokes) {
        for (auto it = srcGroup->strokes().constBegin(); it != srcGroup->strokes().constEnd(); ++it) {
            Stroke *stroke = m_strokes.value(it.key()).get();
            for (const Interval &interval : it.value()) {
                unsigned int newId = dst->pullMaxStrokeIdx();
                StrokePtr newStroke = std::make_shared<Stroke>(*stroke, newId, interval.from(), interval.to());
                // deform the new stroke with the target configuration of the srcGroup lattice
                for (unsigned int i = interval.from(); i <= interval.to(); ++i) {
                    UVInfo uv = srcGroup->uvs().get(it.key(), i);
                    newStroke->points()[i - interval.from()]->pos() = srcGroup->lattice()->getWarpedPoint(newStroke->points()[i - interval.from()]->pos(), uv.quadKey, uv.uv, TARGET_POS);
                }
                editor->undoStack()->push(new DrawCommand(editor, layer, frame, newStroke, Group::ERROR_ID, false));
                newStrokes.push_back(newId);
            }
        }
    };

    editor->undoStack()->beginMacro("Copy group");

    // copy srcGroup into dst (pre group)
    std::vector<int> newStrokesIds;
    copyStrokes(newStrokesIds);
    editor->undoStack()->push(new AddGroupCommand(editor, layer, frame, PRE));
    Group *newPreGroup = dst->preGroups().lastGroup();
    for (int id : newStrokesIds) newPreGroup->addStroke(id);
    editor->undoStack()->push(new SetCorrespondenceCommand(editor, layer, currentFrame, frame, srcGroup->id(), newPreGroup->id()));
    
    // copy srcGroup into dst (post group)
    newStrokesIds.clear();
    copyStrokes(newStrokesIds);
    editor->undoStack()->push(new AddGroupCommand(editor, layer, frame, POST));
    Group *newPostGroup = dst->postGroups().lastGroup();
    for (int id : newStrokesIds) newPostGroup->addStroke(id);

    // lattice and uv stuff
    newPostGroup->setColor(srcGroup->color());
    dst->addIntraCorrespondence(newPreGroup->id(), newPostGroup->id());
    newPostGroup->setGrid(new Lattice(*srcGroup->lattice()));
    newPostGroup->lattice()->setKeyframe(dst);
    for (Corner *c : srcGroup->lattice()->corners()) {
        int key = c->getKey();
        newPostGroup->lattice()->corners()[key]->coord(REF_POS) = newPostGroup->lattice()->corners()[key]->coord(TARGET_POS);
    }

    // rebake stroke intervals in the lattice quads
    newPostGroup->strokes().forEachInterval([&](const Interval &interval, unsigned int strokeID) { 
        editor->grid()->bakeStrokeInGrid(newPostGroup->lattice(), dst->stroke(strokeID), interval.from(), interval.to());
    });
    // rebake UVS
    for (auto it = newPostGroup->strokes().begin(); it != newPostGroup->strokes().end(); ++it) {
        Stroke *stroke = dst->stroke(it.key());
        for (Interval &interval : it.value()) {
            newPostGroup->lattice()->bakeForwardUV(stroke, interval, newPostGroup->uvs());
        }
    }    

    // dirty both the previous and new group lattices
    newPostGroup->lattice()->setArapDirty();
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

void VectorKeyFrame::createBreakdown(Editor *editor, VectorKeyFrame *newKeyframe, VectorKeyFrame *nextKeyframe, const Inbetween& inbetweenCopy, int inbetween, float alpha) {
    if (newKeyframe == nullptr) return;

    // retrieve the baked inbetween strokes
    QHash<int, int> backwardStrokesMapping;
    newKeyframe->strokes() = inbetweenCopy.strokes;
    newKeyframe->m_maxStrokeIdx = m_maxStrokeIdx; 
    int backwardStart = newKeyframe->m_maxStrokeIdx;

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
        group->makeBreakdown(newKeyframe, nextKeyframe, newGroup, inbetween, alpha, globalRigidTransform, backwardStrokesMapping, editor);

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

    newKeyframe->updateBounds(nullptr);
    makeInbetweensDirty();
    newKeyframe->makeInbetweensDirty();
}

/**
 * Add a corresponding pre group to the given post group
 * The new pre group is filled with all stroke segments from the next keyframe that intersect the post group grid
*/
void VectorKeyFrame::makeNextPreGroup(Editor *editor, Group *post) {
    auto it = std::find(editor->layers()->indices().begin(), editor->layers()->indices().end(), m_layer->id());
    if (it == editor->layers()->indices().end()) qCritical() << "Error in makeNextPreGroup: invalid layer!";
    int layer = it - editor->layers()->indices().begin();
    VectorKeyFrame *next = nextKeyframe();
    int currentFrame = m_layer->getVectorKeyFramePosition(this);
    int nextFrame = m_layer->getVectorKeyFramePosition(next);
    if (nextFrame == m_layer->getMaxKeyFramePosition() || post->type() != POST) return;

    // Select strokes segments in the next KF that overlaps with the deformed grid
    Group *newPreGroup = nullptr;
    StrokeIntervals selection;
    // int prevK = INT_MAX;
    // editor->selection()->selectStrokeSegments(next, [&post, &prevK](Point *point) {
    //     QuadPtr q; int k; bool res;
    //     res = post->lattice()->contains(point->pos(), TARGET_POS, q, k);
    //     res = res && (prevK == INT_MAX || post->lattice()->areQuadsConnected(k, prevK));
    //     prevK = k; 
    //     return res;
    // }, selection);

    editor->undoStack()->beginMacro("Make pre group");

    // // Clone all selected segments as new strokes, these new strokes are added to the keyframe but not to a group
    // StrokeIntervals newSelection;
    // for (auto it = selection.constBegin(); it != selection.constEnd(); ++it) {
    //     Stroke *stroke = next->stroke(it.key());
    //     for (const Interval &interval : it.value()) {
    //         unsigned int newId = next->pullMaxStrokeIdx();
    //         editor->undoStack()->push(new DrawCommand(editor, layer, nextFrame, std::make_shared<Stroke>(*stroke, newId, interval.from(), interval.to()), Group::ERROR_ID, false));
    //         newSelection[newId].append(Interval(0, interval.to() - interval.from()));
    //     }
    // }
    // selection.swap(newSelection);

    qDebug() << "next->strokes = " << next->strokes().size();
    for (Group *group : next->postGroups()) {
        for (auto it = group->strokes().constBegin(); it != group->strokes().constEnd(); ++it) {
            Stroke* stroke = next->stroke(it.key());
            auto [startIdx, endIdx] = editor->grid()->addStrokeToDeformedGrid(post->lattice(), stroke);
            if (startIdx == -1 || endIdx == -1) continue;
            // Bake stroke elements in quads and clean grid
            unsigned int newId = next->pullMaxStrokeIdx();
            StrokePtr copiedStroke = std::make_shared<Stroke>(*stroke, newId, startIdx, endIdx);
            editor->undoStack()->push(new DrawCommand(editor, layer, nextFrame, copiedStroke, Group::ERROR_ID, false));
            editor->grid()->bakeStrokeInGrid(post->lattice(), copiedStroke.get(), 0, copiedStroke->size() - 1, TARGET_POS);
            post->lattice()->deleteEmptyVolatileQuads();
            selection[newId].append(Interval(0, copiedStroke->size() - 1));
        }
    }
    qDebug() << "next->strokes after = " << next->strokes().size();

    editor->undoStack()->push(new AddGroupCommand(editor, layer, nextFrame, PRE)); 
    newPreGroup = next->preGroups().lastGroup();
    editor->undoStack()->push(new SetGroupCommand(editor, layer, nextFrame, selection, newPreGroup->id(), PRE));
    editor->undoStack()->push(new SetCorrespondenceCommand(editor, layer, currentFrame, nextFrame, post->id(), newPreGroup->id()));
    editor->undoStack()->endMacro();
}

float VectorKeyFrame::getNextGroupHue() { 
    float prev = m_currentGroupHue; 
    m_currentGroupHue = std::fmod(m_currentGroupHue + 0.618033988749895, 1.0); 
    return prev;
}