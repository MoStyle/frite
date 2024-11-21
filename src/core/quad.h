#ifndef QUAD_H
#define QUAD_H

#include <memory>
#include <bitset>
#include <QHash>
#include <QTransform>
#include "point.h"
#include "strokeinterval.h"

class Corner;

typedef enum { TARGET_POS = 0, REF_POS, INTERP_POS, DEFORM_POS, NUM_COORDS } PosTypeIndex;

typedef enum {
    PIVOT = 0,  // Whether the quad is only there to avoid a "pivot" singularity (to ensure the grid is manifold)
    PINNED,     // Whether the quad is pinned during the matching process
    DIRTY_QUAD,
    UNUSED2,
    UNUSED3,
    MISC3_QUAD,
    MISC2_QUAD,
    MISC_QUAD   // Used for storing temporary states, may be overwritten, only store temporary states!
} QuadFlags;

/**
 * A cell of the lattice
 * Keeps track of the stroke intervals embedded in it (StrokeIntervals)
 */
class Quad {
   public:
    Quad() : m_key(-1) { clear(); }
    Quad(int key) : m_key(key) { clear(); }

    void clear();
    void removeStroke(int strokeId);

    int key() const { return m_key; }
    void setKey(int k) { m_key = k; }
    double averageEdgeLength(PosTypeIndex pos) const;
    
    bool miscFlag() const { return m_flags.test(MISC_QUAD); }
    bool isPivot() const { return m_flags.test(PIVOT); }
    bool isPinned() const { return m_flags.test(PINNED); }
    bool flag(int flag) const { return m_flags.test(flag); }
    const std::bitset<8> &flags() const { return m_flags; }
    void setMiscFlag(bool flag) { m_flags.set(MISC_QUAD, flag); }
    void setPivot(bool pivot) { m_flags.set(PIVOT, pivot); }
    void setFlag(QuadFlags flag, bool val) { m_flags.set(flag, val); }
    void setFlags(const std::bitset<8> &flags) { m_flags = flags; }

    Point::VectorType pinPos() const { return m_pinPosition; }
    Point::VectorType pinUV() const { return m_pinUV; }
    void pin(const Point::VectorType &uv);
    void pin(const Point::VectorType &uv, const Point::VectorType &pos);
    void setPinPosition(const Point::VectorType &newPos) { m_pinPosition = newPos; }
    void unpin();

    Point::VectorType centroid(PosTypeIndex type) const { return m_centroid[type]; }
    Point::VectorType biasedCentroid(PosTypeIndex type) const;
    void computeCentroid(PosTypeIndex type);
    void computeCentroids();
    Point::VectorType getPoint(const Point::VectorType &uv, PosTypeIndex type) const;
    Point::Affine optimalRigidTransform(PosTypeIndex source, PosTypeIndex target);
    Point::Affine optimalAffineTransform(PosTypeIndex source, PosTypeIndex target);
    Point::Affine optimalAffineTransformFromOriginalQuad(int x, int y, int cellSize, Eigen::Vector2i origin, PosTypeIndex target);

    // StrokeIntervals
    int nbForwardStrokes() const { return m_forwardStrokes.size(); }
    int nbBackwardStrokes() const { return m_backwardStrokes.size(); }
    const Intervals forwardStroke(int strokeId) { return m_forwardStrokes.value(strokeId); }
    const Intervals backwardStroke(int strokeId) { return m_backwardStrokes.value(strokeId); }
    const StrokeIntervals &forwardStrokes() const { return m_forwardStrokes; }
    const StrokeIntervals &backwardStrokes() const { return m_backwardStrokes; }
    StrokeIntervals &forwardStrokes() { return m_forwardStrokes; }  // TMP!!
    void setForwardStrokes(const StrokeIntervals &elements) { m_forwardStrokes = elements; }
    void setBackwardStrokes(const StrokeIntervals &elements) { m_backwardStrokes = elements; }
    void addForward(int strokeId, const Intervals &e) { m_forwardStrokes[strokeId].append(e); }
    void addForward(int strokeId, const Interval &e) { m_forwardStrokes[strokeId].append(e); }
    void addBackward(int strokeId, const Intervals &e) { m_backwardStrokes[strokeId].append(e); }
    void addBackward(int strokeId, const Interval &e) { m_backwardStrokes[strokeId].append(e); }
    void insert(int strokeId, const Intervals &e) { m_forwardStrokes.insert(strokeId, e); }
    bool contains(int strokeId) const { return m_forwardStrokes.contains(strokeId); }
    bool isEmpty() { return m_forwardStrokes.isEmpty(); }

    Corner *corners[4];  // pointers to the 4 corners of the quad (public for legacy reasons)
   private:
    int m_key;                                              // quad ID (see lattice posToKey)
    std::bitset<8> m_flags;                                 // store the quad boolean properties
    StrokeIntervals m_forwardStrokes, m_backwardStrokes;    // stroke intervals embedded in this quad
    Point::VectorType m_centroid[4];                        // quad centroid in its different configuration (REF, TARGET, ...)
    Point::VectorType m_pinPosition;                        // position of the pin in the canvas
    Point::VectorType m_pinUV;                              // barycentric coordinate of the pin in the quad
};

typedef std::shared_ptr<Quad> QuadPtr;

#endif  // QUAD_H