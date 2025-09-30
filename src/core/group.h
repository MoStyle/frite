/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef GROUP_H
#define GROUP_H

#include <QBrush>
#include <QColor>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QPainter>
#include <QPen>
#include <QRect>
#include <QTransform>
#include <QHash>
#include <QOpenGLShaderProgram>
#include <functional>

#include "point.h"
#include "polyline.h"
#include "strokeinterval.h"
#include "lattice.h"
#include "keyframedparams.h"
#include "uvhash.h"
#include "bezier2D.h"
#include "partial.h"

enum GroupType : unsigned int { PRE, POST, MAIN };

class VectorKeyFrame;
class Editor;

class Group {
   public:
    Group(VectorKeyFrame *keyframe, GroupType type);
    Group(VectorKeyFrame *keyframe, const QColor &color, GroupType type);
    Group(const Group& other);
    ~Group();

    static void reset();
    void loadStrokes(QDomElement &strokesElt, uint size);
    void load(QDomNode &groupNode);
    void save(QDomDocument &doc, QDomElement &groupsElt) const;
    void update();
    void makeBreakdown(VectorKeyFrame *newKeyframe, VectorKeyFrame *nextKeyframe, Group *breakdown, int inbetween, qreal linearAlpha, const Point::Affine &rigidTransform, const QHash<int, int> &backwardStrokesMap, Editor *editor);
    void clear();

    // Strokes
    Intervals &addStroke(int id, Intervals intervals);      // override current value
    Interval &addStroke(int id, Interval interval);         // append
    Interval &addStroke(int id);                            // override current value
    void clearStrokes();
    void clearStrokes(unsigned int strokeId, bool updateLattice=true);
    void clearStrokes(unsigned int strokeId, unsigned int partialId, bool updateLattice=true);
    const StrokeIntervals &strokes() const { return m_drawingPartials.firstPartial().strokes(); }
    StrokeIntervals &strokes() { return m_drawingPartials.firstPartial().strokes(); }
    StrokeIntervals &strokes(double t) { return m_drawingPartials.lastPartialAt(t).strokes(); }
    Partials<DrawingPartial> &drawingPartials() { return m_drawingPartials; }
    size_t size(double t=0.0) const { return m_drawingPartials.constLastPartialAt(t).strokes().size(); }
    int nbPoints(double t=0.0) const { return m_drawingPartials.constLastPartialAt(t).strokes().nbPoints(); }
    bool contains(unsigned int strokeId) const;
    bool contains(unsigned int strokeId, double t) const { return m_drawingPartials.constLastPartialAt(t).strokes().contains(strokeId); }
    void updateBuffers() const;

    // Drawing stuff
    void drawMask(QOpenGLShaderProgram *program, int inbetween, qreal alpha, QColor color);
    void drawWithoutGrid(QPainter &painter, QPen &pen, qreal alpha, float tintFactor, const QColor &tint, bool useGroupColor=false);
    void drawGrid(QPainter &painter, int inbetween, PosTypeIndex type=TARGET_POS);
    void drawHull(QPainter &painter) const;
    void drawBbox(QPainter &painter) const;
    qreal crossFadeValue(qreal alpha, bool forward);

    // bounding box
    void recomputeBbox();
    void refreshBbox(int id);
    QRectF &bounds() { return m_bbox; }
    const QRectF &cbounds() const { return m_bbox; }
    
    // lattice
    Lattice *lattice() const { return m_grid.get(); }
    void setGrid(Lattice *grid) { m_grid.reset(grid); }
    void clearLattice();
    void clearLattice(int strokeId);
    bool showGrid() const { return m_showGrid; };
    void setShowGrid(bool _showgrid) { m_showGrid = _showgrid; };
    const UVHash &uvs() const { return m_forwardUVs; }
    UVHash &uvs() { return m_forwardUVs; }
    const UVHash &backwardUVs() const { return m_backwardUVs; }
    UVHash &backwardUVs() { return m_backwardUVs; }
    bool breakdown() const { return m_breakdown; }
    void setBreakdown(bool breakdown);
    bool disappear() const { return m_disappear; }
    void setDisappear(bool disappear) { m_disappear = disappear; }
    bool isSticker() const { return m_sticker; }
    void setSticker(bool sticker) { m_sticker = sticker; }
    void setGridDirty();
    void syncTargetPosition(VectorKeyFrame *next);
    void syncSourcePosition(VectorKeyFrame *prev);
    void syncSourcePosition();
    Mask *mask() const { return m_mask.get(); }

    // interpolation transform
    KeyframedReal *spacing() { return m_spacing; }
    KeyframedReal *prevSpacing() { return m_prevSpacing; }
    void setSpacing(KeyframedReal *spacing);
    Point::Affine forwardTransform(qreal linear_alpha, bool useSpacingIndirection = true);
    Point::Affine backwardTransform(qreal linear_alpha);
    qreal spacingAlpha(qreal alpha);
    void computeSpacingProxy(Bezier2D &proxy) const;
    void transform(const Point::Affine &transform);
    Point::Affine rigidTransform(qreal t) const;
    KeyframedVector * getPivot() const { return m_pivot; }
    KeyframedVector * getTranslation() const { return &(m_transform->translation); }
    KeyframedReal * getRotation() const { return &(m_transform->rotation); }
    Point::Affine globalRigidTransform(qreal t) const;
    void applyRotation(float angle, qreal t);
    double motionEnergy() const;
    double motionEnergyStart() const;
    double motionEnergyEnd() const;

    VectorKeyFrame *getParentKeyframe() const { return m_parentKeyframe; }
    void setParentKeyframe(VectorKeyFrame *keyframe) { m_parentKeyframe = keyframe; }

    // correspondence
    Group *prevPostGroup() const;
    Group *prevPreGroup() const;
    Group *nextPreGroup() const;
    Group *nextPostGroup() const;
    int prevPreGroupId() const { return m_prevPreGroupId; }
    void setPrevPreGroupId(int id) { m_prevPreGroupId = id; }

    // id & misc
    int id() const { return m_id; }
    QString nodeNameId() { return m_nodeNameId; }

    // group color
    QColor color() const { return m_color; }
    void setColor(const QColor &color) { m_color = color; }

    // copy of original strokes
    size_t nbOriginStrokes() { return m_origin_strokes.size(); }
    Stroke *originStroke(int id) { return m_origin_strokes[id]; }
    void initOriginStrokes();
    void resetOriginStrokes();
    std::vector<Stroke *> &originStrokes() { return m_origin_strokes; }
    const std::vector<Stroke *> &originStrokes() const { return m_origin_strokes; }

    // intermediate strokes
    inline size_t nbInterStrokes() const { return m_inter_strokes.size(); }
    inline std::vector<Stroke *> &interStrokes(size_t frame) { return m_inter_strokes[frame]; }
    inline void addInterStrokes(std::vector<Stroke *> &_strokes) { m_inter_strokes.push_back(_strokes); }
    void resetInterStrokes();
    bool isShowInterStroke() { return m_showInterStroke; };
    void setShowInterStroke(bool _inter) { m_showInterStroke = _inter; };
    bool isInterpolated() { return m_interpolated; };
    void setInterpolated(bool _inter) { m_interpolated = _inter; };

    // candidate strokes
    inline size_t nbCanStrokes() const { return m_candidate_strokes.size(); }
    inline const std::vector<Stroke *> &canStrokes(size_t frame) { return m_candidate_strokes[frame]; }
    inline void addCanStrokes(std::vector<Stroke *> &_strokes) { m_candidate_strokes.push_back(_strokes); }
    void resetCanStrokes();

    const GroupType &type() const { return m_type;}
    Stroke *stroke(int id) const;

    static const int MAIN_GROUP_ID;
    static const int ERROR_ID;

   private:
    void resetKeyframedParam();

    int m_id;
    GroupType m_type;
    QString m_nodeNameId;

    VectorKeyFrame *m_parentKeyframe;

    //StrokeIntervals m_strokes;  // map strokes id to a list of idx intervals, qhash is supposedly faster than std::map
    Partials<DrawingPartial> m_drawingPartials;

    // Misc. properties and bounds
    QColor m_color, m_initColor;
    QRectF m_bbox;

    // interpolation transform
    KeyframedReal *m_spacing, *m_prevSpacing;

    KeyframedVector *m_pivot;              // center of rotation of the drawing
    KeyframedTransform *m_transform;        // rigid transform of the drawing

    // lattice
    std::shared_ptr<Lattice> m_grid;
    UVHash m_forwardUVs, m_backwardUVs;
    bool m_showGrid;        // true if the lattice can be displayed
    bool m_breakdown;
    bool m_disappear;
    bool m_sticker;
    int m_prevPreGroupId;   // only valid if m_type == POST, cache for VectorKeyFrame m_intraCorrespondences

    // Mask
    std::unique_ptr<Mask> m_mask, m_maskBackward;
    float m_maskStrength;

    // interpolation results
    std::vector<Stroke *> m_origin_strokes;
    std::vector<std::vector<Stroke *>> m_inter_strokes;
    std::vector<std::vector<Stroke *>> m_candidate_strokes;
    bool m_showInterStroke{false};
    bool m_interpolated{false}; 
};

#endif
