/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef VECTOR_KEYFRAME_H
#define VECTOR_KEYFRAME_H

#include "keyframe.h"
#include "stroke.h"
#include "group.h"
#include "grouplist.h"
#include "selection.h"
#include "inbetweens.h"
#include "trajectory.h"

#include <QBrush>
#include <QColor>
#include <QPainter>
#include <QPen>
#include <QtXml>
#include <QOpenGLShaderProgram>

class Point;
class Group;
class GroupList;
class Editor;

class VectorKeyFrame : public KeyFrame {
   public:
    VectorKeyFrame(Layer *m_layer);
    ~VectorKeyFrame();
    void clear();

    // Strokes
    StrokePtr addStroke(const StrokePtr &, Group *group, bool resample=true);
    void removeLastStroke();
    void removeStroke(Stroke *stroke, bool free=true);
    void removeStroke(unsigned int id, bool free=true);
    Stroke *stroke(unsigned int id) const;
    size_t nbStrokes() const { return m_strokes.size(); }
    QHash<int, StrokePtr> &strokes() { return m_strokes; }
    const QHash<int, StrokePtr> &strokes() const { return m_strokes; }
    void updateBuffers();
    void destroyBuffers();

    // Inbetweens
    void computeIntermediateStrokes(float alpha, QHash<int, StrokePtr> &intermediateStrokes, QHash<int, StrokePtr> &backwardIntermediateStrokes, QHash<int, std::vector<Point::VectorType>> &corners) const;
    void clearInbetweens();
    void initInbetweens(int stride);
    void bakeAllInbetweens(Editor *editor);
    void bakeInbetween(Editor *editor, int frame, int inbetween, int stride);
    void updateInbetween(Editor *editor, size_t i);
    const Inbetweens &inbetweens() const { return m_inbetweens; }
    const Inbetween &inbetween(unsigned int inbetweenIdx) const { return m_inbetweens[inbetweenIdx]; }
    const QHash<int, StrokePtr> &inbetweenStrokes(unsigned int inbetweenIdx) const { return m_inbetweens[inbetweenIdx].strokes; }
    const QHash<int, std::vector<Point::VectorType>> &inbetweenCorners(unsigned int inbetweenIdx) const { return m_inbetweens[inbetweenIdx].corners; }
    void makeInbetweensDirty() { m_inbetweens.makeDirty(); }

    // Groups
    inline Group *selectedGroup(GroupType type = POST) const { return type == POST ? (m_selection.selectedPostGroups().empty() ? nullptr : m_selection.selectedPostGroups().begin().value()) 
                                                                                   : (m_selection.selectedPreGroups().empty() ? nullptr : m_selection.selectedPreGroups().begin().value()); }
    inline Group *defaultGroup() { return m_postGroups.fromId(Group::MAIN_GROUP_ID); }
    inline GroupList &groups(GroupType type) { return type == POST ? m_postGroups : m_preGroups; }
    inline GroupList &preGroups() { return m_preGroups; }
    inline GroupList &postGroups() { return m_postGroups; }
    inline const GroupList &groups(GroupType type) const { return type == POST ? m_postGroups : m_preGroups; }
    inline const GroupList &preGroups() const { return m_preGroups; }
    inline const GroupList &postGroups() const { return m_postGroups; }

    // Correspondences 
    const QHash<int, int>& correspondences() const { return m_correspondences; }
    const QHash<int, int>& intraCorrespondences() const { return m_intraCorrespondences; }
    void addCorrespondence(int postGroupId, int preGroupId) { m_correspondences.insert(postGroupId, preGroupId); }
    void addIntraCorrespondence(int preGroupId, int postGroupId);
    void removeCorrespondence(int postGroupId) { m_correspondences.remove(postGroupId); }
    void removeIntraCorrespondence(int preGroupId);
    void clearCorrespondences() { m_correspondences.clear(); }
    void clearIntraCorrespondences();
    VectorKeyFrame *nextKeyframe() { return m_layer->getNextKey(this); }
    VectorKeyFrame *prevKeyframe() { return m_layer->getPrevKey(this); }

    // Drawing
    virtual void paintImage(QPainter &painter, float alpha, int inbetween, const QColor &color, qreal tintFactor, bool useGroupColor = false);
    void paintImageGL(QOpenGLShaderProgram *program, QOpenGLFunctions *functions, float alpha, qreal opacityAlpha, int inbetween, const QColor &color, qreal tintFactor, bool useGroupColor = false);
    void paintImageGL(QOpenGLShaderProgram *program, QOpenGLFunctions *functions, qreal opacityAlpha, const QColor &color, qreal tintFactor, bool useGroupColor = false, GroupType type=POST);
    void paintGroup(QPainter &painter, Group *group, int inbetween, const QColor &color, qreal tintFactor, bool useGroupColor = false);
    void paintGroupGL(QOpenGLShaderProgram *program, QOpenGLFunctions *functions, float alpha, double opacityAlpha, Group *group, int inbetween, const QColor &color, double tintFactor, double strokeWeightFactor=1.0,  bool useGroupColor = false, bool crossFade = true);
    void paintGroupGL(QOpenGLShaderProgram *program, QOpenGLFunctions *functions, double opacityAlpha, Group *group,const QColor &color, double tintFactor, double strokeWeightFactor=1.0,  bool useGroupColor = false);

    // Saving/loading
    virtual bool load(QDomElement &element, const QString &path, Editor *editor);
    virtual bool save(QDomDocument &doc, QDomElement &root, const QString &path, int layer, int frame) const;

    // Global rigid transform
    const Point::VectorType& pivot() const { return m_pivot; }
    void setPivot(const Point::VectorType& pivot) { m_pivot = pivot; }
    KeyframedVector *translation() const { return &(m_transform->translation); }
    KeyframedFloat *rotation() const { return &(m_transform->rotation); }
    KeyframedTransform *keyframedTransform() const { return m_transform; }
    KeyframedFloat *spacing() const { return m_spacing; }
    void toggleAlignFrameToTangent() { m_alignFrameToTangent = !m_alignFrameToTangent; };
    Point::Affine rigidTransform(float t) const;
    void resetRigidDeformation();

    // Trajectories
    unsigned int addTrajectoryConstraint(const std::shared_ptr<Trajectory> &traj);
    void removeTrajectoryConstraint(unsigned int id);
    void connectTrajectories(const std::shared_ptr<Trajectory> &trajA, std::shared_ptr<Trajectory> trajB, bool connectWithNext);     // A=this KF   B=next/prev KF
    void disconnectTrajectories(const std::shared_ptr<Trajectory> &trajA, std::shared_ptr<Trajectory> trajB);  // A=this KF   B=next/prev KF
    Trajectory *trajectoryConstraintPtr(unsigned int idx) const { return m_trajectories.value(idx).get(); }
    std::shared_ptr<Trajectory> trajectoryConstraint(unsigned int idx) const { return m_trajectories.value(idx); }
    unsigned int nbTrajectoryConstraints() const { return m_trajectories.size(); }
    const QHash<unsigned int, std::shared_ptr<Trajectory>>& trajectories() const { return m_trajectories; }
    void resetTrajectories();
    void toggleHardConstraint(bool on);
    void updateCurves();

    // Misc
    const Selection &selection() const { return m_selection; }
    Selection &selection() { return m_selection; }

    void updateBounds(Stroke *stroke);
    virtual void transform(QRectF newBoundaries, bool smoothTransform);
    Layer *parentLayer() const { return m_layer; }
    int parentLayerId() const;

    VectorKeyFrame *copy();
    VectorKeyFrame *copy(const QRectF &selection) const;
    void copyDeformedGroup(VectorKeyFrame *dst, Group *srcGroup);
    void paste(VectorKeyFrame *);
    void paste(VectorKeyFrame *, const QRectF &target);

    void resetInterStrokes();
    void initOriginStrokes();
    void resetOriginStrokes();
    
    void createBreakdown(Editor *editor, VectorKeyFrame *newKeyframe, VectorKeyFrame *nextKeyframe, const Inbetween& inbetweenCopy, int inbetween, float alpha);
    void makeNextPreGroup(Editor *editor, Group *post);

    float getNextGroupHue();
    unsigned int maxStrokeIdx() const { return m_maxStrokeIdx; }
    unsigned int pullMaxStrokeIdx() { return m_maxStrokeIdx++; }
    unsigned int pullMaxConstraintIdx() { return m_maxConstraintIdx++; }

   private:
    Layer *m_layer;                         // parent layer of this KF

    float m_currentGroupHue;                // used for cycling group color
    unsigned int m_maxStrokeIdx;            // used for indexing strokes
    unsigned int m_maxConstraintIdx;        // used for indexing constraints

    QHash<int, StrokePtr> m_strokes;        // all strokes in the keyframe
    Inbetweens m_inbetweens;                // baked inbetweens between this keyframe and the next. The first inbetween is at idx 0 and the last stored inbetween is t=1.0!
    
    GroupList m_preGroups;                  // segmentation of m_strokes used for backward interpolation
    GroupList m_postGroups;                 // segmentation of m_strokes used for forward interpolation

    Selection m_selection;
    QHash<int, int> m_correspondences;      // correspondonce post->pre with the next KF
    QHash<int, int> m_intraCorrespondences; // correspondence pre->post inside this KF

    Point::VectorType m_pivot;              // center of rotation of the drawing
    KeyframedTransform *m_transform;        // rigid transform of the drawing
    KeyframedFloat *m_spacing;              // spacing curve: controls the flow of time for the forward interpolation
    bool m_alignFrameToTangent;             // should the drawing be aligned with the tangent its trajectory
    QHash<unsigned int, std::shared_ptr<Trajectory>> m_trajectories; // trajectory constraints
};

#endif
