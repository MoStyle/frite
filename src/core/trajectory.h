#ifndef __TRAJECTORY_H__
#define __TRAJECTORY_H__

#include "keyframedparams.h"
#include "uvhash.h"
#include "bezier2D.h"
#include <QPainterPath>
#include <QPainter>
#include <memory.h>

class VectorKeyFrame;
class Group;
class Lattice;

class Trajectory {
public:
    Trajectory(VectorKeyFrame *keyframe, Group *group, const UVInfo &latticeCoord, bool sample=true);
    Trajectory(const Trajectory &other);

    void sampleTrajectory();
    void update();
    void clampLocalSpacing();
    void split(Point::Scalar t, const std::shared_ptr<Trajectory> &rightHalf);

    void setGroup(Group *group) { m_group = group; }
    void setCubicApprox(const Bezier2D &newApprox) { m_cubicApprox = newApprox; updatePathItem(false);}
    void setConstraintID(unsigned int constraintID) { m_constraintID = constraintID; }
    void setHardConstraint(bool hardConstraint) { m_hardConstraint = hardConstraint; }
    void setPrevTrajectory(std::shared_ptr<Trajectory> prev) { m_prevTrajectory = prev; }
    void setNextTrajectory(std::shared_ptr<Trajectory> next) { m_nextTrajectory = next; }
    void setSyncPrev(bool syncPrev) { m_syncPrev = syncPrev; }
    void setSyncNext(bool syncNext) { m_syncNext = syncNext; }
    void setP1(Point::VectorType p1) { m_cubicApprox.setP1(p1); updatePathItem(false); } 
    void setP2(Point::VectorType p2) { m_cubicApprox.setP2(p2); updatePathItem(false); } 
    void setQuadKey(int newKey) { m_latticeCoord.quadKey = newKey; }
    void forceUpdateCubicApprox() { m_cubicApprox.updateArclengthLUT(); }
    void addChild(const std::shared_ptr<Trajectory> &child);
    void setParent(const std::shared_ptr<Trajectory> &parent);
    void copyParent();
    void adjustLocalOffsetFromParent();
    void adjustLocalOffsetFromContuinityConstraint();
    void resetLocalOffset();

    Point::VectorType eval(double t) { return m_fitArap ? m_cubicApprox.eval(t) : m_cubicApprox.evalArcLength(t); }
    Point::VectorType evalVelocity(double t) { return m_fitArap ? m_cubicApprox.evalDer(t) : m_cubicApprox.evalDerArcLength(t); }
    Point::VectorType key(size_t i);
    Point::VectorType keyTangent(size_t i, unsigned int side);

    size_t nbKeys() const { return m_curve->curve()->nbPoints(); }
    VectorKeyFrame *keyframe() const { return m_keyframe; }
    Group *group() const { return m_group; }
    const UVInfo &latticeCoord() const { return m_latticeCoord; }
    KeyframedVector *curve() const { return m_curve.get(); }
    const Bezier2D &cubicApprox() const { return m_cubicApprox; }
    KeyframedReal *localOffset() const { return m_offset.get(); }
    unsigned int constraintID() const { return m_constraintID; }
    bool hardConstraint() const { return m_hardConstraint; }
    const std::shared_ptr<Trajectory> &nextTrajectory() const { return m_nextTrajectory; }
    const std::shared_ptr<Trajectory> &prevTrajectory() const { return m_prevTrajectory; }
    const std::shared_ptr<Trajectory> &parentTrajectory() const { return m_parentTrajectory; }
    const std::vector<std::shared_ptr<Trajectory>> &childrenTrajectories() const { return m_childrenTrajectories; }
    bool syncPrev() const { return m_syncPrev; }
    bool syncNext() const { return m_syncNext; }
    int prevTrajectoryID() const { return m_prevTrajectoryID; }
    int nextTrajectoryID() const { return m_nextTrajectoryID; }
    int parentTrajectoryID() const { return m_parentTrajectoryId; }
    const std::vector<int> childrenTrajectoriesIds() const { return m_childrenIds; }

    const QPainterPath &pathItem() const { return m_pathItem; } 
    const QPainterPath &approxPathItem() const { return m_approxPathItem; } 
    const QPainterPath &approxPathHull() const { return m_approxPathHullItem; }

    void updatePathItem(bool fitArap=false);

    static std::shared_ptr<Trajectory> load(QDomElement &el, VectorKeyFrame *key);
    void save(QDomDocument &doc, QDomElement &el, const VectorKeyFrame *key) const;
private:

    VectorKeyFrame *m_keyframe;
    Group *m_group;
    Lattice *m_grid;
    UVInfo m_latticeCoord;                          // coordinate inside the lattice

    std::unique_ptr<KeyframedVector> m_curve;       // animation curve of the trajectory (DEPRECATED!)
    Bezier2D m_cubicApprox;                         // cubic bezier segment approximating the trajectory
    std::unique_ptr<KeyframedReal> m_offset;       // local spacing offset
    unsigned int m_constraintID;                    // id of the constraint in the lattice (only if this is a hard constraint)
    bool m_hardConstraint;                          // whether or not this trajectory is a hard constraint
    bool m_fitArap;                                 // special case when the cubic bezier approximate the resut of an arap interpolation

    std::shared_ptr<Trajectory> m_prevTrajectory;   // corresponding trajectory on the next pair of KF (can be null)
    std::shared_ptr<Trajectory> m_nextTrajectory;   // corresponding trajectory on the previous pair of KF (can be null)
    std::shared_ptr<Trajectory> m_parentTrajectory; // if nullptr then the trajectory is parent
    std::vector<std::shared_ptr<Trajectory>> m_childrenTrajectories;
    bool m_syncPrev, m_syncNext;                    // whether or not the tangents with the next/prev trajectories are the same
    int m_prevTrajectoryID, m_nextTrajectoryID;
    int m_parentTrajectoryId;
    std::vector<int> m_childrenIds;

    // Graphics items
    QPainterPath m_pathItem;
    QPainterPath m_approxPathItem;
    QPainterPath m_approxPathHullItem;              // oversized hull for picking
};

#endif // __TRAJECTORY_H__