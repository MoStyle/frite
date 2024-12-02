#include "trajectory.h"

#include "vectorkeyframe.h"
#include "group.h"
#include "uvhash.h"
#include "dialsandknobs.h"

#include <QPainterPathStroker>

static dkInt k_trajectoryMinRes("Options->Trajectory->Min resolution", 1, 0, 50, 1);

Trajectory::Trajectory(VectorKeyFrame *keyframe, Group *group, const UVInfo &latticeCoord, bool sample) 
    : m_keyframe(keyframe), 
      m_group(group), 
      m_latticeCoord(latticeCoord),
      m_syncNext(false),
      m_syncPrev(false) {
    if (m_group != nullptr) m_grid = m_group->lattice();
    m_curve = std::make_unique<KeyframedVector>("Trajectory");    
    m_curve->setInterpolation("Trajectory", Curve::HERMITE_INTERP);
    m_offset = std::make_unique<KeyframedReal>("Local offset");
    m_offset->setInterpolation("Local offset", Curve::SPLINE_INTERP);
    m_offset->set(0.0);
    m_offset->addKey("Local offset", 0.0);
    m_offset->addKey("Local offset", 1.0);
    m_hardConstraint = false;
    m_fitArap = false;
    if (sample) sampleTrajectory();
}

Trajectory::Trajectory(const Trajectory &other) 
    : m_keyframe(other.m_keyframe),
      m_group(other.m_group),
      m_grid(other.m_grid),
      m_latticeCoord(other.m_latticeCoord),
      m_cubicApprox(other.m_cubicApprox),
      m_constraintID(other.m_constraintID),
      m_hardConstraint(other.m_hardConstraint),
      m_fitArap(other.m_fitArap),
      m_prevTrajectory(other.m_prevTrajectory),
      m_nextTrajectory(other.m_nextTrajectory),
      m_parentTrajectory(other.m_parentTrajectory), 
      m_childrenTrajectories(other.m_childrenTrajectories),
      m_syncPrev(other.m_syncPrev),
      m_syncNext(other.m_syncNext),
      m_approxPathItem(other.m_approxPathItem),
      m_approxPathHullItem(other.m_approxPathHullItem) {
    m_curve = std::make_unique<KeyframedVector>("Trajectory");    
    m_curve->setInterpolation("Trajectory", Curve::HERMITE_INTERP);
    m_offset = std::make_unique<KeyframedReal>("Local offset");
    m_offset->setInterpolation("Local offset", Curve::SPLINE_INTERP);
    m_offset->set(0.0);
    m_offset->addKey("Local offset", 0.0);
    m_offset->addKey("Local offset", 1.0);
    for (int i = 1; i < other.m_offset->curve()->nbPoints() - 1; ++i) {
        m_offset->set(other.m_offset->curve()->point(i).y());
        m_offset->addKey("Local offset", other.m_offset->curve()->point(i).x());
    }
}

void Trajectory::sampleTrajectory() {
    Layer *layer = m_keyframe->parentLayer();
    int inbetween = layer->inbetweenPosition(layer->getVectorKeyFramePosition(m_keyframe));
    int stride = layer->stride(layer->getVectorKeyFramePosition(m_keyframe));

    // fit cubic to arap interpolation (with respect to the arap interpolation parameterization!)
    std::vector<Point::VectorType> data;
    std::vector<Point::Scalar> u;
    qreal alpha;

    if (m_grid->isArapPrecomputeDirty()) m_grid->precompute();

    for (int i = 0; i < 12; ++i) {
        alpha = (float) i / 11.0f;
        float remappedAlpha = m_group->spacingAlpha(alpha);
        m_grid->interpolateARAP(alpha, remappedAlpha, m_group->globalRigidTransform(alpha), false);
        u.push_back(remappedAlpha);
        data.push_back(m_grid->getWarpedPoint(Point::VectorType::Zero(), m_latticeCoord.quadKey, m_latticeCoord.uv, INTERP_POS));
    }
    m_cubicApprox.fitWithParam(data, u);
    m_fitArap = true;

    m_offset->curve()->resample(m_keyframe->parentLayer()->stride(m_keyframe->parentLayer()->getVectorKeyFramePosition(m_keyframe)) - 1);

    // add control points to the animation curve by sampling the ARAP interpolation
    // use baked inbetweens if there are more than the required minimum amount of control points
    m_curve->removeKeys("Trajectory");
    m_curve->set(m_group->lattice()->getWarpedPoint(Point::VectorType::Zero(), m_latticeCoord.quadKey, m_latticeCoord.uv, REF_POS));
    m_curve->addKey("Trajectory", 0.0f);
    m_curve->set(m_group->lattice()->getWarpedPoint(Point::VectorType::Zero(), m_latticeCoord.quadKey, m_latticeCoord.uv, TARGET_POS));
    // m_curve->set(m_keyframe->inbetweens().back().getWarpedPoint(m_group, m_latticeCoord));
    m_curve->addKey("Trajectory", 1.0f);
    m_curve->curve(0)->smoothTangents();
    m_curve->curve(1)->smoothTangents();
    float remappedAlpha;
    for (int i = 1; i < k_trajectoryMinRes; ++i) {
        alpha = (float) i / (float) k_trajectoryMinRes;
        remappedAlpha = m_group->spacingAlpha(alpha);
        m_grid->interpolateARAP(alpha, remappedAlpha, m_group->globalRigidTransform(alpha), false);
        m_curve->set(m_grid->getWarpedPoint(Point::VectorType::Zero(), m_latticeCoord.quadKey, m_latticeCoord.uv, INTERP_POS));
        m_curve->addKey("Trajectory", alpha);
    }
    m_curve->curve(0)->smoothTangents();
    m_curve->curve(1)->smoothTangents();

    updatePathItem(true);
}

void Trajectory::update() {
    if (!m_hardConstraint) {
        sampleTrajectory();
    } else {
        // resample current curve without chaning keyframes values
        // or remove hard constraint 
    }
}

void Trajectory::split(Point::Scalar t, const std::shared_ptr<Trajectory> &rightHalf) {
    rightHalf->m_latticeCoord = m_latticeCoord;
    t = m_fitArap ? t : m_cubicApprox.param(t); 
    Bezier2D l, r;
    m_cubicApprox.split(t, l, r);
    setCubicApprox(l);
    rightHalf->setCubicApprox(r);
}

void Trajectory::addChild(const std::shared_ptr<Trajectory> &child) {
    if (m_parentTrajectory != nullptr) qCritical() << "Error in addChild: cannot add a child to a child trajectory!";
    m_childrenTrajectories.push_back(child);
}

void Trajectory::setParent(const std::shared_ptr<Trajectory> &parent) {
    m_parentTrajectory = parent;
    copyParent();
}

void Trajectory::copyParent() {
    if (m_parentTrajectory == nullptr) return;

    m_cubicApprox = m_parentTrajectory->cubicApprox();
    m_hardConstraint = m_parentTrajectory->hardConstraint();
    m_fitArap = m_parentTrajectory->m_fitArap;
    m_prevTrajectory = m_parentTrajectory->prevTrajectory();
    m_nextTrajectory = m_parentTrajectory->nextTrajectory();
    m_syncPrev = m_parentTrajectory->syncPrev();
    m_syncNext = m_parentTrajectory->syncNext();
    m_approxPathItem = m_parentTrajectory->approxPathItem();
    m_approxPathHullItem = m_parentTrajectory->approxPathHull();
    m_pathItem = m_parentTrajectory->pathItem();

    adjustLocalOffsetFromParent();
}

void Trajectory::adjustLocalOffsetFromParent() {
    if (m_parentTrajectory == nullptr) return;
    int nbPoints = m_group->spacing()->curve()->nbPoints();

    if (m_parentTrajectory->group()->spacing()->curve()->nbPoints() != nbPoints) {
        qCritical() << "Error in adjustLocalOffsetFromParent: invalid spacing";
    }

    if (m_offset->curve()->nbPoints() != nbPoints) {
        m_offset->curve()->resample(nbPoints - 2);
    }

    Group *parentGroup = m_parentTrajectory->group();
    double spacingParent, spacingGroup, x;
    for (int i = 1; i < nbPoints - 1; ++i) {
        x = m_group->spacing()->curve()->point(i).x();
        spacingParent = parentGroup->spacing()->curve()->point(i).y();
        spacingGroup = m_group->spacing()->curve()->point(i).y();
        m_offset->curve()->setKeyframe(Eigen::Vector2d(x, spacingParent - spacingGroup), i);
    }
}

void Trajectory::adjustLocalOffsetFromContuinityConstraint() {
    if (!m_hardConstraint || m_nextTrajectory == nullptr || !m_syncNext) {
        return;
    }

    Group *curGroup = m_group;
    Group *nextGroup = m_nextTrajectory->group();
    Curve *curCurve = curGroup->spacing()->curve();
    Curve *nextCurve = nextGroup->spacing()->curve();
    Bezier2D curProxy, nextProxy;
    m_cubicApprox.updateArclengthLUT();
    m_nextTrajectory->forceUpdateCubicApprox();
    curGroup->computeSpacingProxy(curProxy);
    nextGroup->computeSpacingProxy(nextProxy);
    // TODO fix nbPoints and length
    Point::VectorType tangentCurProxy = (curProxy.getP3() - curProxy.getP2()).cwiseProduct(Point::VectorType(curCurve->nbPoints(), m_cubicApprox.length()));
    Point::VectorType tangentNextProxy = (nextProxy.getP1() - curProxy.getP0()).cwiseProduct(Point::VectorType(nextCurve->nbPoints(), m_nextTrajectory->cubicApprox().length()));
    Point::VectorType newTangent = (tangentCurProxy + tangentNextProxy) * 0.5;
    curProxy.setP2(curProxy.getP3() - (newTangent.cwiseProduct(Point::VectorType(1.0 / curCurve->nbPoints(), 1.0 / m_cubicApprox.length()))));
    nextProxy.setP1(curProxy.getP0() + (newTangent.cwiseProduct(Point::VectorType(1.0 / nextCurve->nbPoints(), 1.0 / m_nextTrajectory->cubicApprox().length()))));

    for (int j = 1; j < m_offset->curve()->nbPoints() - 1; ++j) {
        Eigen::Vector2d p = m_offset->curve()->point(j);
        m_offset->curve()->setKeyframe(Eigen::Vector2d(p.x(), curProxy.evalYFromX(p.x()) - curCurve->evalAt(p.x())), j);
    }
    for (int j = 1; j < m_nextTrajectory->localOffset()->curve()->nbPoints() - 1; ++j) {
        Eigen::Vector2d p = m_nextTrajectory->localOffset()->curve()->point(j);
        m_nextTrajectory->localOffset()->curve()->setKeyframe(Eigen::Vector2d(p.x(), nextProxy.evalYFromX(p.x()) - nextCurve->evalAt(p.x())), j);
    }
}

void Trajectory::resetLocalOffset() {
    for (int i = 0; i < m_offset->curve()->nbPoints(); ++i) {
        Eigen::Vector2d p = m_offset->curve()->point(i);
        m_offset->curve()->setKeyframe(Eigen::Vector2d(p.x(), 0.0f), i);
    }
}

Point::VectorType Trajectory::key(size_t i) {
    return Point::VectorType(m_curve->curve(0)->point(i).y(), m_curve->curve(1)->point(i).y());
}

Point::VectorType Trajectory::keyTangent(size_t i, unsigned int side) {
    if (side == 0) {
        return Point::VectorType(m_curve->curve(0)->tangent(i).head<2>().y(), m_curve->curve(1)->tangent(i).head<2>().y()); // + (right side of key point)

    } else {
        return Point::VectorType(m_curve->curve(0)->tangent(i).tail<2>().y(), m_curve->curve(1)->tangent(i).tail<2>().y()); // - (left side of key point)
    }
}

void Trajectory::updatePathItem(bool fitArap) {
    m_pathItem.clear();

    m_fitArap = fitArap;
    m_cubicApprox.updateArclengthLUT();

    unsigned int nb = nbKeys();

    Point::VectorType pos = key(0);
    Point::VectorType c1, c2;
    m_pathItem.moveTo(pos.x(), pos.y());
    for (unsigned int i = 1; i < nb; ++i) {
        c1 = pos + keyTangent(i - 1, 0);
        pos = key(i);
        c2 = pos + keyTangent(i, 1);
        m_pathItem.cubicTo(c1.x(), c1.y(), c2.x(), c2.y(), pos.x(), pos.y());
    }

    m_approxPathItem.clear();
    m_approxPathItem.moveTo(m_cubicApprox.getP0().x(), m_cubicApprox.getP0().y());
    m_approxPathItem.cubicTo(m_cubicApprox.getP1().x(), m_cubicApprox.getP1().y(), m_cubicApprox.getP2().x(), m_cubicApprox.getP2().y(), m_cubicApprox.getP3().x(), m_cubicApprox.getP3().y());

    QPainterPathStroker stroker(QPen(QBrush(Qt::black), 10.0));
    m_approxPathHullItem = stroker.createStroke(m_approxPathItem);

    if (!m_childrenTrajectories.empty()) {
        for (const std::shared_ptr<Trajectory> &child : m_childrenTrajectories) {
            child->copyParent();
        }
    }
}

std::shared_ptr<Trajectory> Trajectory::load(QDomElement &trajEl, VectorKeyFrame *key) {
    int groupID = trajEl.attribute("groupID").toInt();
    Group *group = key->postGroups().fromId(groupID);
    UVInfo latticeCoord;    
    latticeCoord.quadKey = trajEl.attribute("quadKey").toInt();
    latticeCoord.uv.x() = trajEl.attribute("u").toDouble();
    latticeCoord.uv.y() = trajEl.attribute("v").toDouble();
    std::shared_ptr<Trajectory> traj = std::make_shared<Trajectory>(key, group, latticeCoord, false);

    traj->m_constraintID = trajEl.attribute("id").toInt();
    traj->m_hardConstraint = trajEl.attribute("hardConstraint").toInt();
    traj->m_syncNext = trajEl.attribute("syncNext").toInt();
    traj->m_syncPrev = trajEl.attribute("syncPrev").toInt();
    traj->m_nextTrajectoryID = trajEl.attribute("nextTrajID", "-1").toInt();
    traj->m_prevTrajectoryID = trajEl.attribute("prevTrajID", "-1").toInt();
    traj->m_parentTrajectoryId = trajEl.attribute("parentId", "-1").toInt();

    QDomElement bezierElt = trajEl.firstChildElement("bezier2D");
    traj->m_cubicApprox.load(bezierElt);

    QDomElement localOffsetEl = trajEl.firstChildElement("localOffset");
    traj->m_offset->load(localOffsetEl);

    QDomElement childrenIds = trajEl.firstChildElement("childrenIds");
    if (!childrenIds.isNull()) {
        int size = childrenIds.attribute("size").toInt();
        QString childrenString = childrenIds.text();
        QTextStream childrenStream(&childrenString);
        int id;
        for (int i = 0; i < size; ++i) {
            childrenStream >> id;
            traj->m_childrenIds.push_back(id);
        }
    }

    traj->updatePathItem();
    return traj;
}

void Trajectory::save(QDomDocument &doc, QDomElement &el, const VectorKeyFrame *key) const {
    QDomElement trajElt = doc.createElement("traj");
    trajElt.setAttribute("id", (int)m_constraintID);
    trajElt.setAttribute("groupID", (int)m_group->id());
    trajElt.setAttribute("hardConstraint", (int)m_hardConstraint);
    if (m_nextTrajectory != nullptr) trajElt.setAttribute("nextTrajID", (int)m_nextTrajectory->constraintID());
    if (m_prevTrajectory != nullptr) trajElt.setAttribute("prevTrajID", (int)m_prevTrajectory->constraintID());
    trajElt.setAttribute("syncNext", (int)m_syncNext);
    trajElt.setAttribute("syncPrev", (int)m_syncPrev);
    if (m_parentTrajectory != nullptr) trajElt.setAttribute("parentId", (int)m_parentTrajectory->constraintID());

    QString string;
    QTextStream stream(&string);

    // save lattice coord
    trajElt.setAttribute("quadKey", (int)m_latticeCoord.quadKey);
    trajElt.setAttribute("u", (double)m_latticeCoord.uv.x());
    trajElt.setAttribute("v", (double)m_latticeCoord.uv.y());

    // save cubic approx
    m_cubicApprox.save(doc, trajElt);

    // save local offset
    QDomElement offsetElt = doc.createElement("localOffset");
    m_offset->save(doc, offsetElt);
    trajElt.appendChild(offsetElt);

    // save children ids
    QString childrenString;
    QTextStream childrenStream(&childrenString);
    QDomElement childrenIds = doc.createElement("childrenIds");
    childrenIds.setAttribute("size", (int)m_childrenTrajectories.size());
    for (const std::shared_ptr<Trajectory> child : m_childrenTrajectories) {
        childrenStream << child->constraintID() << " ";
    }
    QDomText childrenTxt = doc.createTextNode(childrenString);
    childrenIds.appendChild(childrenTxt);
    trajElt.appendChild(childrenIds);

    el.appendChild(trajElt);
}