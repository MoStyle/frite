/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef LATTICE_H
#define LATTICE_H

#include <QHash>
#include <QTransform>
#include <QVector>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLExtraFunctions>
#include <iostream>
#include <set>

#include <Eigen/Geometry>
#include <Eigen/SparseCore>
#include <Eigen/SparseLU>

#include "corner.h"
#include "layer.h"
#include "point.h"
#include "quad.h"
#include "logarithmicspiral.h"

class VectorKeyFrame;
class Group;
class Stroke;
class UVHash;
class Mask;

using namespace Eigen;
class Lattice {
   public:
    Lattice(VectorKeyFrame *keyframe);
    Lattice(const Lattice &other);
    Lattice(const Lattice &other, const std::vector<int> &quads);

    void init(int cellsize, int nbCols, int nbRows, Eigen::Vector2i origin = Eigen::Vector2i::Zero());
    void save(QDomDocument &doc, QDomElement &latticeElt) const;
    void load(QDomElement &latticeElt);
    void clear();
    void removeStroke(int strokeId, bool breakdown);

    // Lattice params
    int cellSize() const { return m_cellSize; }
    void setCellSize(int size) { m_cellSize = size; }
    int nbCols() const { return m_nbCols; }
    void setNbCols(int col) { m_nbCols = col; }
    int nbRows() const { return m_nbRows; }
    void setNbRows(int row) { m_nbRows = row; }
    Eigen::Vector2i origin() const { return m_oGrid; }
    void setOrigin(const Eigen::Vector2i &origin) { m_oGrid = origin; }
    void setKeyframe(VectorKeyFrame *keyframe) { m_keyframe = keyframe; }
    int getMaxCornerKey() { return m_maxCornerKey; }
    void setMaxCornerKey(int k) { m_maxCornerKey = k; }
    double scale() const { return m_scale; }
    void setScale(double scale) { m_scale = scale; }
    Point::Affine scaling() const { return m_scaling; }
    void setScaling(Point::Affine scaling) { m_scaling = scaling; }

    // Flags
    inline bool isArapPrecomputeDirty() const { return m_precomputeDirty; }
    inline bool isArapInterpDirty() const { return m_arapDirty; }
    inline bool needRetrocomp() const { return m_retrocomp; }
    inline bool isBufferCreated() const { return m_bufferCreated; }
    void setArapDirty();
    inline float currentPrecomputedTime() const { return m_currentPrecomputedTime; }
    inline void resetPrecomputedTime() { m_currentPrecomputedTime = -1; }

    // Quads & corners
    inline bool contains(int key) const { return m_quads.contains(key); }
    inline void insert(int key, QuadPtr cell) { m_quads.insert(key, cell); }
    inline int size() const { return m_quads.size(); }
    inline QuadPtr operator[](int key) { return m_quads[key]; }
    inline QuadPtr quad(int key) const { return m_quads.contains(key) ? m_quads.value(key) : nullptr; }
    inline QuadPtr at(int x, int y) { return m_quads[coordToKey(x, y)]; }
    const QHash<int, QuadPtr> &quads() { return m_quads; }
    bool isEmpty() { return m_quads.empty(); }
    inline Corner *find(int key) { return m_corners[key]; }
    inline QVector<Corner *> &corners() { return m_corners; }
    // inline QList<QuadPtr> quads() { return m_quads.values(); }
    inline Point::VectorType refCM() const { return m_refCM; }

    QuadPtr addQuad(const Point::VectorType &point, bool &isNewQuad);
    QuadPtr addQuad(int key, int x, int y, bool &isNewQuad);
    QuadPtr addQuad(QuadPtr &quad);
    QuadPtr addEmptyQuad(int key);
    void deleteQuad(int key);
    void deleteQuadsPredicate(std::function<bool(QuadPtr)> predicate);
    void deleteUnusedCorners();

    // UVs & embedding
    bool backwardUVDirty() const { return m_backwardUVDirty; }
    void setBackwardUVDirty(bool dirty) { m_backwardUVDirty = dirty; }
    bool quadContainsPoint(QuadPtr quad, const Point::VectorType &p, PosTypeIndex cornerType) const;
    bool contains(const Point::VectorType &p, PosTypeIndex cornerType, QuadPtr &quad, int &key) const;
    bool contains(Stroke *stroke, int from, int to, PosTypeIndex pos, bool checkConnectivity=false) const;
    bool contains(VectorKeyFrame *key, const StrokeIntervals &intervals, PosTypeIndex pos, bool checkConnectivity=false) const;
    bool intersects(Stroke *stroke, int from, int to, PosTypeIndex pos) const;
    bool intersects(VectorKeyFrame *key, const StrokeIntervals &intervals, PosTypeIndex pos) const;
    bool intersectedQuads(Stroke *stroke, int from, int to, PosTypeIndex pos, std::set<int> &quads) const;
    bool tagValidPath(Stroke *stroke, int from, int to, PosTypeIndex pos, QuadFlags flag) const;
    Point::VectorType getUV(const Point::VectorType &p, PosTypeIndex type, int &quadKey);
    Point::VectorType getUV(const Point::VectorType &p, PosTypeIndex type, QuadPtr quad);
    Point::VectorType getWarpedPoint(const Point::VectorType &p, int quadKey, const Point::VectorType &uv, PosTypeIndex type);
    bool bakeForwardUV(const Stroke *stroke, Interval &interval, UVHash &uvs, PosTypeIndex type=REF_POS);
    bool bakeForwardUVConnectivityCheck(const Stroke *stroke, Interval &interval, UVHash &uvs, PosTypeIndex type=REF_POS);
    bool bakeForwardUVPrecomputed(const Stroke *stroke, Interval &interval, UVHash &uvs);
    bool bakeBackwardUV(const Stroke *stroke, Interval &interval, const Point::Affine &transform, UVHash &uvs);

    // Trajectory constraints
    bool addConstraint(unsigned int constraintIdx) { return m_constraintsIdx.insert(constraintIdx).second; }
    void removeConstraint(unsigned int constraintIdx) { m_constraintsIdx.erase(constraintIdx); }
    int nbConstraints() const { return m_constraintsIdx.size(); }
    const std::set<unsigned int> &constraints() const { return m_constraintsIdx; }

    // Pins
    std::vector<Point::VectorType> pinsDisplacementVectors() const;
    void displacePinsQuads(PosTypeIndex dstPos);

    // Drawing 
    void drawLattice(QPainter &painter, qreal interpFactor, const QColor &color, PosTypeIndex type = TARGET_POS) const;
    void drawLattice(QPainter &painter, const QColor &color, VectorKeyFrame *keyframe, int groupID, int inbetween) const;
    void drawPins(QPainter &painter);
    void drawCornersTrajectories(QPainter &painter, const QColor &color, Group *group, VectorKeyFrame *key, bool linearInterpolation = true);
    // Must be called with a valid OpenGL context
    void bindVAO() { m_vao.bind(); }
    void releaseVAO() { m_vao.release(); }
    void createBuffer(QOpenGLShaderProgram *program, QOpenGLExtraFunctions *functions);
    void destroyBuffer();
    void updateBuffer();

    // Compute P^T and LHS of ARAP equation (with constraint)
    void precompute();
    // Compute ARAP interpolation and store it in INTERP_POS corner coord
    void interpolateARAP(qreal alphaLinear, qreal alpha, const Point::Affine &globalRigidTransform, bool useRigidTransform = true);

    // Misc. and utils
    void applyTransform(const Point::Affine &transform, PosTypeIndex ref, PosTypeIndex dst);
    void copyPositions(const Lattice *src, PosTypeIndex srcPos, PosTypeIndex dst);          // assume copied lattice
    void moveSrcPosTo(const Lattice *target, PosTypeIndex srcPos, PosTypeIndex targetPos);  // assume copied lattice

    Point::VectorType centerOfGravity(PosTypeIndex type = TARGET_POS);

    Point::VectorType motionEnergy2D() const { return m_tgtCM - m_refCM; };
    Point::VectorType motionEnergy2D(double t) const { return (m_tgtCM * t + m_refCM * (1.0 - t)) - m_refCM; };

    void resetDeformation();

    // Utils
    Point::VectorType projectOnEdge(const Point::VectorType &p, int &outQuad) const;
    bool areQuadsConnected(int quadKeyA, int quadKeyB) const;
    void adjacentQuads(Corner *corner, std::array<int, 4> &neighborKeys);
    void adjacentQuads(QuadPtr quad, std::array<int, 8> &neighborKeys);
    QuadPtr adjacentQuad(QuadPtr quad, EdgeIndex edge);
    bool isSingleConnectedComponent() const { return m_singleConnectedComponent; }
    bool isConnected();
    void getConnectedComponents(std::vector<std::vector<int>> &outputComponents, bool overrideFlag=true);

    Corner *findBoundaryCorner(PosTypeIndex coordType);
    Corner *findNextBoundaryCorner(Corner * corner);
    int quadsInCommon(Corner *c1, Corner *c2);
    void markOutline();

    bool checkPotentialBowtie(Point::VectorType &prevPoint, Point::VectorType &curPoint, int &quadKeyOut);
    void enforceManifoldness(Group *group);
    void enforceManifoldness(Stroke *stroke, Interval &interval, std::vector<QuadPtr> &newQuads, bool forceAddPivots = false);

    void debug(std::ostream &os) const;
    void restoreKeysRetrocomp(Group *group, Editor *editor);

    inline int coordToKey(int x, int y) const { return x + y * m_nbCols; }

    inline void keyToCoord(int key, int &x, int &y) const { 
        x = key % m_nbCols;
        y = key / m_nbCols;
    }

    inline int posToKey(const Point::VectorType &p) const {
        int x, y;
        posToCoord(p, x, y);
        return coordToKey(x, y);
    }

    inline void posToCoord(const Point::VectorType &p, int &x, int &y) const {
        double i = double(p.x() - m_oGrid.x()) / double(m_cellSize);
        double j = double(p.y() - m_oGrid.y()) / double(m_cellSize);
        x = int(floor(i));
        y = int(floor(j));
    }

    void setToRestTransform(Point::Affine transform) { m_toRestPos = transform; };
    Point::Affine getToRestTransform() const { return m_toRestPos; };

   private:
    bool checkQuadsShareStroke(VectorKeyFrame *keyframe, QuadPtr q1, QuadPtr q2, std::vector<QuadPtr> &newQuads);
    void computePStar(QuadPtr q, int cornerI, int cornerJ, int quadRow, bool inverseOrientation, std::vector<Eigen::Triplet<double>> &P_triplets);
    void computeQuadA(QuadPtr q, MatrixXd &At, int &i, float t, bool inverseOrientation);
    
    VectorKeyFrame *m_keyframe;

    QHash<int, QuadPtr> m_quads;
    QVector<Corner *> m_corners;

    int m_nbCols;
    int m_nbRows;
    int m_cellSize;
    Eigen::Vector2i m_oGrid;              // TL of the canvas
    Point::VectorType m_refCM, m_tgtCM;   // center of mass of the lattice in its reference and target positions

    Point::Affine m_toRestPos;
    Point::Affine m_scaling;

    // Constraints indices in the keyframe list
    std::set<unsigned int> m_constraintsIdx;

    // Matrices for ARAP interpolation
    SparseMatrix<double, ColMajor> m_Pt;
    SparseLU<SparseMatrix<double, ColMajor>, COLAMDOrdering<int>> m_LU;
    VectorXd m_W;
    double m_rot, m_scale;

    // Flags and cached stuff
    bool m_precomputeDirty;
    bool m_arapDirty;
    bool m_backwardUVDirty;
    bool m_singleConnectedComponent;
    bool m_retrocomp;
    float m_currentPrecomputedTime;
    int m_maxCornerKey;

    // GL stuff
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo, m_ebo;
    bool m_bufferCreated, m_bufferDestroyed, m_bufferDirty;
};

#endif  // LATTICE_H
