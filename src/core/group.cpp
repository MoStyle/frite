#include "group.h"

#include <math.h>

#include <QDomText>
#include <QString>
#include <QTextStream>
#include <QBrush>

#include "editor.h"
#include "vectorkeyframe.h"
#include "mask.h"
#include "tesselator.h"
#include "gridmanager.h"
#include "viewmanager.h"
#include "utils/geom.h"
#include "dialsandknobs.h"
#include "utils/stopwatch.h"

extern dkBool k_displayGrids;
extern dkBool k_displayMask;
extern dkBool k_useCrossFade;
extern dkBool k_useInterpolation;

const int Group::MAIN_GROUP_ID = -1;
const int Group::ERROR_ID = -2;

Group::Group(VectorKeyFrame *keyframe, GroupType type)
    : m_type(type),
      m_parentKeyframe(keyframe),
      m_drawingPartials(keyframe, DrawingPartial(keyframe, 0.0)),
      m_spacing(new KeyframedReal("Spacing")),
      m_transform(new KeyframedTransform("Transform")),
      m_pivot(new KeyframedVector("Pivot")),
      m_prevSpacing(nullptr),
      m_showGrid(false),
      m_breakdown(false),
      m_disappear(false),
      m_sticker(false),
      m_prevPreGroupId(-1),
      m_mask(std::make_unique<Mask>(this, true)),
      m_maskBackward(std::make_unique<Mask>(this, false)),
      m_maskStrength(1.0) {
    switch (m_type) {
        case POST:
            m_id = m_parentKeyframe->postGroups().curIdx();
            break;
        case PRE:
            m_id = m_parentKeyframe->preGroups().curIdx();
            break;
        case MAIN:
            m_id = MAIN_GROUP_ID;
            m_type = POST;
            break;
        default:
            qCritical() << "Wrong group type";
            break;
    }
    m_color = QColor::fromHslF(keyframe->getNextGroupHue(), 1.0f, 0.5f);
    m_initColor = m_color;
    m_nodeNameId = QString("Group " /* + std::to_string(m_id)*/);
    m_spacing->setInterpolation(m_nodeNameId, Curve::MONOTONIC_CUBIC_INTERP);
    resetKeyframedParam();
}

Group::Group(VectorKeyFrame *keyframe, const QColor &color, GroupType type)
    : m_type(type),
      m_parentKeyframe(keyframe),
      m_drawingPartials(keyframe, DrawingPartial(keyframe, 0.0)),
      m_color(color),
      m_initColor(color),
      m_spacing(new KeyframedReal("Spacing")),
      m_transform(new KeyframedTransform("Transform")),
      m_pivot(new KeyframedVector("Pivot")),
      m_prevSpacing(nullptr),
      m_showGrid(false),
      m_breakdown(false),
      m_disappear(false),
      m_sticker(false),
      m_prevPreGroupId(-1),
      m_mask(std::make_unique<Mask>(this, true)),
      m_maskBackward(std::make_unique<Mask>(this, false)),
      m_maskStrength(1.0) {
    switch (m_type) {
        case POST:
            m_id = m_parentKeyframe->postGroups().curIdx();
            break;
        case PRE:
            m_id = m_parentKeyframe->preGroups().curIdx();
            break;
        case MAIN:
            m_id = -1;
            m_type = POST;
            break;
        default:
            qCritical() << "Wrong group type";
            break;
    }
    m_nodeNameId = QString("Group ");
    m_spacing->setInterpolation(m_nodeNameId, Curve::MONOTONIC_CUBIC_INTERP);
    resetKeyframedParam();
}

Group::Group(const Group &other)
    : m_type(other.m_type),
      m_id(other.m_id),
      m_nodeNameId(other.m_nodeNameId),
      m_parentKeyframe(other.m_parentKeyframe),
      m_drawingPartials(other.m_drawingPartials),
      m_color(other.m_color),
      m_initColor(other.m_initColor),
      m_bbox(other.m_bbox),
      m_spacing(new KeyframedReal(*other.m_spacing)),
      m_transform(new KeyframedTransform("Transform")),
      m_pivot(new KeyframedVector("Pivot")),
      m_forwardUVs(other.m_forwardUVs),
      m_backwardUVs(other.m_backwardUVs),
      m_showGrid(other.m_showGrid),
      m_breakdown(other.m_breakdown),
      m_disappear(other.m_disappear),
      m_sticker(false),
      m_prevPreGroupId(other.m_prevPreGroupId),
      m_mask(std::make_unique<Mask>(this, true)),
      m_maskBackward(std::make_unique<Mask>(this, false)),
      m_maskStrength(other.m_maskStrength) {
    m_prevSpacing = other.m_prevSpacing == nullptr ? nullptr : new KeyframedReal(*other.m_prevSpacing);

    if (other.m_grid != nullptr) {
        setGrid(new Lattice(*other.m_grid));
        setGridDirty();
    }
}

Group::~Group() {
    delete m_spacing;
    delete m_pivot;
    delete m_transform;
    // TODO delete origin strokes, can strokes, etc
}

void Group::reset() {}

void Group::loadStrokes(QDomElement &strokesElt, uint size) {
    m_drawingPartials.firstPartial().strokes().reserve(size);
    QDomNode strokeTag = strokesElt.firstChild();
    while (!strokeTag.isNull()) {
        int strokeId = strokeTag.toElement().attribute("id").toInt();
        int size = strokeTag.toElement().attribute("size").toInt();
        QString stringIntervals = strokeTag.toElement().text();
        QTextStream streamIntervals(&stringIntervals);
        int toIdx, fromIdx;
        for (size_t i = 0; i < size; ++i) {
            streamIntervals >> fromIdx >> toIdx;
            addStroke(strokeId, Interval(fromIdx, toIdx));
        }
        strokeTag = strokeTag.nextSibling();
    }
}

void Group::load(QDomNode &groupNode) {
    QDomElement groupElt = groupNode.toElement();
    m_id = groupElt.attribute("id").toInt();
    m_color = QColor::fromHslF(m_parentKeyframe->getNextGroupHue(), 1.0f, 0.5f);
    m_initColor = m_color;
    uint size = groupElt.attribute("size").toUInt();
    m_breakdown = (bool)groupElt.attribute("breakdown", "0").toInt();
    m_disappear = (bool)groupElt.attribute("disappear", "0").toInt();
    m_sticker = (bool)groupElt.attribute("sticker", "0").toInt();
    m_maskStrength = groupElt.attribute("maskStrength", "1.0").toFloat();

    // load strokes
    QDomElement strokesElt = groupNode.firstChildElement();
    if (!strokesElt.isNull()) {
        loadStrokes(strokesElt, size);
    }

    // load spacing curve
    QDomElement spacingElt = strokesElt.nextSiblingElement("spacing");
    if (!spacingElt.isNull()) m_spacing->load(spacingElt);

    // load lattice
    QDomElement latticeElt = strokesElt.nextSiblingElement("lattice");
    if (!latticeElt.isNull()) {
        setGrid(new Lattice(m_parentKeyframe));
        m_grid->load(latticeElt);
    } else {
        for (auto it = strokes().constBegin(); it != strokes().constEnd(); ++it) {
            Stroke *stroke = m_parentKeyframe->stroke(it.key());
            for (const Interval &interval : it.value()) {
                Interval inter(interval.from(), interval.to());
                m_parentKeyframe->parentLayer()->editor()->grid()->constructGrid(this, m_parentKeyframe->parentLayer()->editor()->view(), stroke, inter);
            }
        }
    }

    // load uvs quad keys
    QDomElement uvQuadKeyElt = strokesElt.nextSiblingElement("uvquadkey");
    if (!uvQuadKeyElt.isNull()) {
        int size = uvQuadKeyElt.attribute("size", "0").toInt();
        QString stringQuadKey = uvQuadKeyElt.text();
        QTextStream posQuadKey(&stringQuadKey);
        unsigned int key;
        int quadKey;
        for (int i = 0; i < size; ++i) {
            posQuadKey >> key >> quadKey;
            m_forwardUVs[key] = {quadKey, Point::VectorType::Zero()};
        }
    }
}

void Group::save(QDomDocument &doc, QDomElement &groupsElt) const {
    QDomElement groupElt = doc.createElement("group");

    // save group attributes
    groupElt.setAttribute("id", m_id);
    groupElt.setAttribute("type", m_type);
    groupElt.setAttribute("size", uint(m_drawingPartials.firstPartial().strokes().size()));
    groupElt.setAttribute("hue", m_color.hueF());
    groupElt.setAttribute("breakdown", (int)m_breakdown);
    groupElt.setAttribute("disappear", (int)m_disappear);
    groupElt.setAttribute("sticker", (int)m_sticker);

    // save strokes intervals
    QDomElement strokesElt = doc.createElement("strokes");
    // const StrokeIntervals &strokes = m_showInterStroke ? m_origin_strokes : m_strokes;
    for (auto it = m_drawingPartials.firstPartial().strokes().constBegin(); it != m_drawingPartials.firstPartial().strokes().constEnd(); it++) {
        QDomElement strokeElt = doc.createElement("stroke");
        QString stringIntervals;
        QTextStream streamIntervals(&stringIntervals);
        strokesElt.appendChild(strokeElt);
        strokeElt.setAttribute("id", it.key());
        strokeElt.setAttribute("size", (unsigned int)it.value().size());
        for (Interval interval : it.value()) streamIntervals << interval.from() << " " << interval.to() << " ";
        QDomText txtIntervals = doc.createTextNode(stringIntervals);
        strokeElt.appendChild(txtIntervals);
    }
    groupElt.appendChild(strokesElt);

    QDomElement spacingElt = doc.createElement("spacing");
    m_spacing->save(doc, spacingElt);
    groupElt.appendChild(spacingElt);

    // save lattice
    if (m_grid != nullptr) {
        QDomElement latticeElt = doc.createElement("lattice");
        m_grid->save(doc, latticeElt);
        groupElt.appendChild(latticeElt);
    }

    // save forward UV quads
    QDomElement uvQuadKeyElt = doc.createElement("uvquadkey");
    uvQuadKeyElt.setAttribute("size", uint(m_forwardUVs.size()));
    QString stringQuadKey;
    QTextStream startPosQuadKey(&stringQuadKey);
    for (auto it = m_forwardUVs.constBegin(); it != m_forwardUVs.constEnd(); ++it) {
        startPosQuadKey << it.key() << " " << it.value().quadKey << " ";
    }
    QDomText txt = doc.createTextNode(stringQuadKey);
    txt = doc.createTextNode(stringQuadKey);
    uvQuadKeyElt.appendChild(txt);
    groupElt.appendChild(uvQuadKeyElt);

    groupsElt.appendChild(groupElt);
}

/**
 * Update the bounding box
 */
void Group::update() {
    if (m_drawingPartials.firstPartial().strokes().empty()) return;
    recomputeBbox();
}

/**
 * 
 * @param newKeyframe               the new breakdown keyframe
 * @param nextKeyframe              the next keyframe (before adding the new breakdown KF)
 * @param breakdown                 the new breakdown group (from newKeyframe)
 * @param inbetween                 where we're adding the breakdown (inbetween nb)
 * @param linearAlpha               where we're adding the breakdown (0<=linearAlpha<=1)
 * @param rigidTransform            interpolated rigid transform of the KF pivot
 * @param backwardStrokesMap        maps the backward stroke id from the nextKeyframe to the newKeyframe id
 * @param editor 
 */
void Group::makeBreakdown(VectorKeyFrame *newKeyframe, VectorKeyFrame *nextKeyframe, Group *breakdown, int inbetween, qreal linearAlpha, const Point::Affine &rigidTransform,
                          const QHash<int, int> &backwardStrokesMap, Editor *editor) {
    if (m_type != POST) return;

    // copy strokes intervals and bounds in this new group
    breakdown->m_drawingPartials.firstPartial().strokes() = m_drawingPartials.firstPartial().strokes();
    breakdown->recomputeBbox();

    // copy stroke UVs
    breakdown->m_forwardUVs = m_forwardUVs;
    breakdown->m_backwardUVs = m_backwardUVs;

    // if this group cross fades with a pre group in the next KF, include the pre group strokes in the new breakdown
    Group *nextPreGrp = nextPreGroup();
    int backwardStart = INT_MAX;
    if (nextPreGrp != nullptr) {
        // copy stroke intervals and copy the backward UVs (now forward for the breakdown)
        for (auto it = nextPreGrp->strokes().constBegin(); it != nextPreGrp->strokes().constEnd(); ++it) {
            if (!backwardStrokesMap.contains(it.key())) qCritical() << "makeBreakdown: backwardStrokesMap does not contain the stroke id " << it.key();
            int newID = backwardStrokesMap.value(it.key());
            Stroke *newStroke = newKeyframe->stroke(newID);
            breakdown->m_drawingPartials.firstPartial().strokes().insert(newID, it.value());
            if (newID < backwardStart) backwardStart = newID;
            for (const Interval &interval : it.value()) {
                for (unsigned int i = interval.from(); i <= interval.to(); ++i) {
                    // update breakdown's forward uvs
                    newStroke->points()[i]->initId(newStroke->id(), i);
                    breakdown->m_forwardUVs.add(newID, i, m_backwardUVs.get(it.key(), i));
                }
            }
        }
        // update strokes width
        for (auto it = breakdown->strokes().begin(); it != breakdown->strokes().end(); ++it) {
            if (it.key() < backwardStart) {
                newKeyframe->stroke(it.key())->setStrokeWidth(newKeyframe->stroke(it.key())->strokeWidth() * crossFadeValue(spacingAlpha(linearAlpha), true));
            } else {
                newKeyframe->stroke(it.key())->setStrokeWidth(newKeyframe->stroke(it.key())->strokeWidth() * crossFadeValue(spacingAlpha(linearAlpha), false));
            }
        }
    }
 

    // create the lattice of the new group with the same topology as the previous group
    // the reference position of the new group and the target position of the previous group are both set to the intermediate lattice position
    m_grid->interpolateARAP(linearAlpha, spacingAlpha(linearAlpha), rigidTransform, false);
    breakdown->setGrid(new Lattice(*m_grid));
    breakdown->lattice()->setKeyframe(newKeyframe);
    for (Corner *c : m_grid->corners()) {
        int key = c->getKey();
        c->coord(TARGET_POS) = c->coord(INTERP_POS);
        c->coord(DEFORM_POS) = c->coord(INTERP_POS);
        // !a corner key id in the previous group does not necessarily correspond to the same corner key in the new group
        // here it doesn't matter since we're using the corners from the same grid in both sides of the followings assigments
        breakdown->lattice()->corners()[key]->coord(REF_POS) = rigidTransform * breakdown->lattice()->corners()[key]->coord(INTERP_POS);
        breakdown->lattice()->corners()[key]->coord(INTERP_POS) = rigidTransform * breakdown->lattice()->corners()[key]->coord(INTERP_POS);
        breakdown->lattice()->corners()[key]->coord(TARGET_POS) = rigidTransform * breakdown->lattice()->corners()[key]->coord(TARGET_POS);
    }

    // rebake stroke intervals in the lattice quads
    breakdown->strokes().forEachInterval(
        [&](const Interval &interval, unsigned int strokeID) { editor->grid()->bakeStrokeInGrid(breakdown->lattice(), newKeyframe->stroke(strokeID), interval.from(), interval.to()); });

    // dirty both the previous and new group lattices
    setGridDirty();

    m_grid->resetPrecomputedTime();
    m_grid->setBackwardUVDirty(true);
    breakdown->setGridDirty();
    breakdown->lattice()->resetPrecomputedTime();
    breakdown->lattice()->setBackwardUVDirty(true);

    // split trajectories
    for (auto &traj : m_parentKeyframe->trajectories()) {
        if (traj->group() != this) continue;
        std::shared_ptr<Trajectory> rightHalf = std::make_shared<Trajectory>(newKeyframe, breakdown, traj->latticeCoord(), false);
        traj->split(spacingAlpha(linearAlpha), rightHalf);
        newKeyframe->addTrajectoryConstraint(rightHalf);
        const std::shared_ptr<Trajectory> &nextTraj = traj->nextTrajectory();
        if (nextTraj != nullptr) {
            m_parentKeyframe->disconnectTrajectories(traj, nextTraj);
            newKeyframe->connectTrajectories(rightHalf, nextTraj, true);
            if (traj->syncNext()) rightHalf->setSyncNext(true);
        }
        traj->setSyncNext(true);
        rightHalf->setSyncPrev(true);
        m_parentKeyframe->connectTrajectories(traj, rightHalf, true);
    }

    // split group's spacing curve
    m_spacing->frameChanged(linearAlpha);
    KeyframedReal *spacingSecondHalf = new KeyframedReal(*m_spacing, inbetween, m_spacing->curve()->nbPoints() - 1);
    breakdown->setSpacing(spacingSecondHalf);
    while (m_spacing->curve()->nbPoints() > inbetween + 1) {
        m_spacing->removeLastPoint();
    }
    m_spacing->normalizeX();

    breakdown->setBreakdown(true);
}

void Group::clear() {
    clearStrokes();
    setGrid(nullptr);
    resetKeyframedParam();
    m_drawingPartials = Partials<DrawingPartial>(m_parentKeyframe, DrawingPartial(m_parentKeyframe, 0.0));
    m_forwardUVs.clear();
    m_backwardUVs.clear();
    m_breakdown = false;
    m_disappear = false;
    m_sticker = false;
}

Intervals &Group::addStroke(int id, Intervals intervals) {
    m_drawingPartials.firstPartial().strokes().insert(id, intervals);  // erase previous value if there are any
    m_drawingPartials.firstPartial().strokes().size() == 1 ? recomputeBbox() : refreshBbox(id);
    if (m_type == POST) {
        for (const Interval &interval : intervals) {
            for(int i = interval.from(); i <= interval.to(); ++i) {
                m_parentKeyframe->stroke(id)->points()[i]->setGroupId(m_id);
            }
        }
    }
    return m_drawingPartials.firstPartial().strokes()[id];
}

Interval &Group::addStroke(int id, Interval interval) {
    m_drawingPartials.firstPartial().strokes()[id].append(interval);
    m_drawingPartials.firstPartial().strokes().size() == 1 ? recomputeBbox() : refreshBbox(id);
    if (m_type == POST) {
        for(int i = interval.from(); i <= interval.to(); ++i) {
            m_parentKeyframe->stroke(id)->points()[i]->setGroupId(m_id);
        }
    }
    return m_drawingPartials.firstPartial().strokes()[id].back();
}

Interval &Group::addStroke(int id) {
    qDebug() << "Adding stroke id : " << id << " / " << m_parentKeyframe->strokes().size() << " in group " << m_id;
    m_drawingPartials.firstPartial().strokes()[id].clear();
    m_drawingPartials.firstPartial().strokes()[id].append(Interval(0, stroke(id)->size() - 1));
    m_drawingPartials.firstPartial().strokes().size() == 1 ? recomputeBbox() : refreshBbox(id);
    if (m_type == POST) {
        for(int i = 0; i < m_parentKeyframe->stroke(id)->size(); ++i) {
            m_parentKeyframe->stroke(id)->points()[i]->setGroupId(m_id);
        }
    }
    return m_drawingPartials.firstPartial().strokes()[id].back();
}

/**
 * Clear strokes in the group (not partials!)
 */
void Group::clearStrokes() {
    m_drawingPartials.firstPartial().strokes().clear();
    clearLattice();
    update();
}

/**
 * Remove the given stroke in all partials 
 */
void Group::clearStrokes(unsigned int strokeId, bool updateLattice) {
    for (DrawingPartial &partial : m_drawingPartials.partials()) {
        if (partial.strokes().contains(strokeId)) {
            partial.strokes().remove(strokeId);
        }
    }
    if (updateLattice) {
        clearLattice(strokeId);
    }
    update();
}

/**
 * Remove the given stroke in the specified partial
 */
void Group::clearStrokes(unsigned int strokeId, unsigned int partialId, bool updateLattice) {
    m_drawingPartials.partial(partialId)->strokes().remove(strokeId);
    if (updateLattice && !contains(strokeId)) {
        clearLattice();
    }
    update();
}

/**
 * Returns true if the group contains the stroke in any drawing partial 
 */
bool Group::contains(unsigned int strokeId) const { 
    for (const DrawingPartial &partial : m_drawingPartials.partials()) { 
        return partial.strokes().contains(strokeId);
    } 
    return false; 
}

void Group::updateBuffers() const {
    for (auto it = m_drawingPartials.firstPartial().strokes().constBegin(); it != m_drawingPartials.firstPartial().strokes().constEnd(); ++it) {
        stroke(it.key())->updateBuffer(m_parentKeyframe);
    }
}

void Group::drawMask(QOpenGLShaderProgram *program, int inbetween, qreal alpha, QColor color) {
    if (!k_useInterpolation) {
        alpha = 0.0f;
        inbetween = 0;
    }

    const Inbetween &inb = m_parentKeyframe->inbetween(inbetween);

    if (m_grid != nullptr && m_grid->size() > 0 && inb.fullyVisible.value(m_id) && m_grid->isConnected()) {
        Group *next = nextPreGroup();
        qreal spacingAlpha = this->spacingAlpha(alpha);
        bool drawNext = next != nullptr && k_useCrossFade;
        float strengthForward =  drawNext ? crossFadeValue(spacingAlpha, true)  : m_maskStrength;
        float strengthBackward = drawNext ? crossFadeValue(spacingAlpha, false) : m_maskStrength; 
        if (m_disappear) strengthForward = std::max(1.0 - spacingAlpha, 0.0);
        if (drawNext && size() == 0) strengthBackward = std::max(spacingAlpha, 0.0);
        program->setUniformValue("groupColor", color);

        // Forward
        if (m_mask->isDirty()) m_mask->computeOutline();
        program->setUniformValue("maskStrength", (float)(strengthForward));
        m_mask->createBuffer(program, m_parentKeyframe, inbetween); // TODO: bake the buffer in the inbetween
        m_mask->bindVAO();
        StopWatch s("Draw mask");
        glDrawElements(GL_TRIANGLES, tessGetElementCount(m_mask->tessellator()) * 3, GL_UNSIGNED_INT, nullptr);
        s.stop();
        m_mask->releaseVAO();
        m_mask->destroyBuffer();

        // Backward (if crossfade)
        if (drawNext) {
            if (m_maskBackward->isDirty()) m_maskBackward->computeOutline();
            program->setUniformValue("maskStrength", (float)(strengthBackward));
            m_maskBackward->createBuffer(program, m_parentKeyframe, inbetween);
            m_maskBackward->bindVAO();
            glDrawElements(GL_TRIANGLES, tessGetElementCount(m_maskBackward->tessellator()) * 3, GL_UNSIGNED_INT, nullptr);
            m_maskBackward->releaseVAO();
            m_maskBackward->destroyBuffer();
        }
    }
}

void Group::drawWithoutGrid(QPainter &painter, QPen &pen, qreal alpha, float tintFactor, const QColor &tint, bool useGroupColor) {
    auto tintColor = [](Stroke *stroke, float tintFactor, QColor color) {
        return QColor(int((stroke->color().redF() * (100.0 - tintFactor) + color.redF() * tintFactor) * 2.55),
                      int((stroke->color().greenF() * (100.0 - tintFactor) + color.greenF() * tintFactor) * 2.55),
                      int((stroke->color().blueF() * (100.0 - tintFactor) + color.blueF() * tintFactor) * 2.55), 255);
    };
    painter.save();
    painter.setTransform(globalRigidTransform(alpha).toQTransform(), true);

    // draw strokes
    for (auto it = m_drawingPartials.firstPartial().strokes().begin(); it != m_drawingPartials.firstPartial().strokes().end(); ++it) {
        Stroke *stroke = this->stroke(it.key());
        if (stroke == nullptr) {
            qWarning() << "trying to draw null stroke (group " << m_id << ")";
            continue;
        }
        if (tintFactor > 0)
            pen.setColor(tintColor(stroke, tintFactor, tint));
        else
            pen.setColor(stroke->color());
        if (useGroupColor) pen.setColor(m_color);
        for (auto itIntervals = it.value().begin(); itIntervals != it.value().end(); ++itIntervals) {
            Interval &interval = *itIntervals;
            stroke->draw(painter, pen, interval.from(), interval.to());
        }
    }
    painter.restore();
}

void Group::drawGrid(QPainter &painter, int inbetween, PosTypeIndex type) {
    if (m_showGrid && m_grid != nullptr && k_displayGrids) {
        // if (type == REF_POS) m_color = m_parentKeyframe->parentLayer()->editor()->backwardColor();
        // else m_color = m_parentKeyframe->parentLayer()->editor()->forwardColor();
        if (type == INTERP_POS) {
            // if (inbetween == 0 && type != REF_POS) {
            //     type = REF_POS;
            //     m_color = m_parentKeyframe->parentLayer()->editor()->backwardColor();
            // }
            m_grid->drawLattice(painter, m_color, m_parentKeyframe, m_id, inbetween);
        } else {
            m_grid->drawLattice(painter, 1.0f, m_color, type);
        }
    }
}

void Group::recomputeBbox() {
    qreal min = std::numeric_limits<qreal>::lowest();
    qreal max = std::numeric_limits<qreal>::max();
    qreal minX = max, maxX = min, minY = max, maxY = min;
    qreal maxRadius = -1.0;
    for (auto it = m_drawingPartials.firstPartial().strokes().constBegin(); it != m_drawingPartials.firstPartial().strokes().constEnd(); ++it) {
        m_drawingPartials.firstPartial().strokes().forEachPoint(
            m_parentKeyframe,
            [&](Point *point) {
                if (point->x() < minX) minX = point->x();
                if (point->x() > maxX) maxX = point->x();
                if (point->y() < minY) minY = point->y();
                if (point->y() > maxY) maxY = point->y();
            },
            it.key());

        if (stroke(it.key())->strokeWidth() + 10 > maxRadius) maxRadius = stroke(it.key())->strokeWidth() + 10;
    }
    m_bbox.setTopLeft(QPointF(minX, minY));
    m_bbox.setBottomRight(QPointF(maxX, maxY));
}

void Group::refreshBbox(int id) {
    qreal minX = m_bbox.left();
    qreal minY = m_bbox.top();
    qreal maxX = m_bbox.right();
    qreal maxY = m_bbox.bottom();

    m_drawingPartials.firstPartial().strokes().forEachPoint(
        m_parentKeyframe,
        [&](Point *point) {
            if (point->x() < minX) minX = point->x();
            if (point->x() > maxX) maxX = point->x();
            if (point->y() < minY) minY = point->y();
            if (point->y() > maxY) maxY = point->y();
        },
        id);

    m_bbox.setTopLeft(QPointF(minX, minY));
    m_bbox.setBottomRight(QPointF(maxX, maxY));
}

void Group::drawBbox(QPainter &painter) const {
    QPen pen(QBrush(Qt::gray, Qt::SolidPattern), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.drawRect(m_bbox);
}

qreal Group::crossFadeValue(qreal alpha, bool forward) {
    qreal x = 2 * (alpha - 0.5);
    qreal xSq = 1 - x * x;
    if (forward) {
        return alpha > 0.5 ? (xSq * xSq) : 1.0;
    }
    return alpha > 0.5 ? 1.0 : (xSq * xSq);
}

void Group::setSpacing(KeyframedReal *spacing) {
    delete m_spacing;
    m_spacing = spacing;
}

Point::Affine Group::forwardTransform(qreal linear_alpha, bool useSpacingIndirection) { return Point::Affine::Identity(); }

Point::Affine Group::backwardTransform(qreal linear_alpha) { return Point::Affine::Identity(); }

qreal Group::spacingAlpha(qreal alpha) {
    m_spacing->frameChanged(alpha);
    qreal res = m_spacing->get();
    return res < std::numeric_limits<qreal>::epsilon() ? 0.0 : res;
}

void Group::computeSpacingProxy(Bezier2D &proxy) const {
    // Create data to fit
    Curve *curve = m_spacing->curve();
    int nbPoints = std::max(4, (int)curve->nbPoints());

    std::vector<Point::VectorType> data(nbPoints);
    data[0] = Point::VectorType::Zero();
    for (int i = 1; i < nbPoints - 1; ++i) {
        double x = (double)i / nbPoints;
        data[i] = Point::VectorType(x, curve->evalAt(x));
    }
    data[nbPoints - 1] = Point::VectorType::Ones();

    proxy.fit(data, true);

    // clamp proxy to unit square (guarantees monotony)
    proxy.setP0(Point::VectorType::Zero());
    proxy.setP1(Point::VectorType(std::clamp(proxy.getP1().x(), 0.0, 1.0), std::clamp(proxy.getP1().y(), 0.0, 1.0)));
    proxy.setP2(Point::VectorType(std::clamp(proxy.getP2().x(), 0.0, 1.0), std::clamp(proxy.getP2().y(), 0.0, 1.0)));
    proxy.setP3(Point::VectorType::Ones());
}

void Group::transform(const Point::Affine &transform) {
    m_drawingPartials.firstPartial().strokes().forEachPoint(m_parentKeyframe, [&](Point *point) { point->pos() = transform * point->pos(); });
}

Point::Affine Group::rigidTransform(qreal t) const{
    m_spacing->frameChanged(t);
    t = m_spacing->get();

    m_pivot->frameChanged(t);
    Point::VectorType pivot = m_pivot->get();
    m_transform->frameChanged(t);

    Point::VectorType tangent = m_transform->translation.getDerivative();

    Point::Translation translation(m_transform->translation.get());
    Point::Rotation rotation(m_transform->rotation.get()); 
    Point::Translation toPivot(-pivot);

    return Point::Affine(translation * toPivot.inverse() * rotation * toPivot);
}

Point::Affine Group::globalRigidTransform(qreal t) const {
    m_spacing->frameChanged(t);
    float tVectorKeyFrame = m_spacing->get();
    return getParentKeyframe()->rigidTransform(tVectorKeyFrame) * rigidTransform(t); 
}


void Group::applyRotation(float angle, qreal t){
    return;
    // m_transform->rotation.set(angle);
    // m_transform->rotation.addKey("Rotation", t);
}

double Group::motionEnergy() const { return m_grid->motionEnergy2D().norm(); };

double Group::motionEnergyStart() const { return m_grid->motionEnergy2D(m_spacing->curve()->point(1).y()).norm(); };

double Group::motionEnergyEnd() const { return m_grid->motionEnergy2D().norm() - m_grid->motionEnergy2D(m_spacing->curve()->point(m_spacing->curve()->nbPoints() - 2).y()).norm(); };

Group *Group::prevPostGroup() const {
    switch (m_type) {
        case POST: {
            Group *prevPreGrp = prevPreGroup();
            if (prevPreGrp == nullptr) return nullptr;
            return prevPreGrp->prevPostGroup();
        }
        case PRE: {
            VectorKeyFrame *prevKey = m_parentKeyframe->prevKeyframe();
            if (prevKey == nullptr || prevKey == m_parentKeyframe) return nullptr;
            int postGroupId = prevKey->correspondences().key(m_id, Group::ERROR_ID);
            if (postGroupId == Group::ERROR_ID) return nullptr;
            return prevKey->postGroups().fromId(postGroupId);
        }
        default:
            return nullptr;
    }
}

Group *Group::prevPreGroup() const {
    switch (m_type) {
        case POST: {
            int preGroupKey = m_parentKeyframe->intraCorrespondences().key(m_id, Group::ERROR_ID);
            if (preGroupKey == Group::ERROR_ID) return nullptr;
            return m_parentKeyframe->preGroups().fromId(preGroupKey);
        }
        case PRE: {
            Group *prevPostGrp = prevPostGroup();
            if (prevPostGrp == nullptr) return nullptr;
            return prevPostGrp->prevPreGroup();
        }
        default:
            return nullptr;
    }
}

Group *Group::nextPreGroup() const {
    switch (m_type) {
        case POST: {
            if (!m_parentKeyframe->correspondences().contains(m_id)) return nullptr;
            int nextGroupId = m_parentKeyframe->correspondences().value(m_id);
            return m_parentKeyframe->nextKeyframe()->preGroups().fromId(nextGroupId);
        }
        case PRE: {
            Group *nextPostGrp = nextPostGroup();
            if (nextPostGrp == nullptr) return nullptr;
            return nextPostGrp->nextPreGroup();
        }
        default:
            return nullptr;
    }
}

Group *Group::nextPostGroup() const {
    switch (m_type) {
        case POST: {
            Group *nextPreGrp = nextPreGroup();
            if (nextPreGrp == nullptr) return nullptr;
            return nextPreGrp->nextPostGroup();
        }
        case PRE: {
            if (!m_parentKeyframe->intraCorrespondences().contains(m_id)) return nullptr;
            int nextPostGroupId = m_parentKeyframe->intraCorrespondences().value(m_id);
            return m_parentKeyframe->postGroups().fromId(nextPostGroupId);
        }
        default:
            return nullptr;
    }
}

void Group::resetKeyframedParam() {
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
    m_transform->rotation.setInterpolation("Rotation", Curve::HERMITE_INTERP);
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

void Group::initOriginStrokes() {
    qDeleteAll(m_origin_strokes);
    m_origin_strokes.clear();
    // for (Stroke *s : m_strokes) m_origin_strokes.push_back(new Stroke(*s));
    // TODO: copy strokes intervals as new strokes
}

void Group::resetOriginStrokes() {
    // m_strokes.clear();
    // for (size_t i = 0; i < nbOriginStrokes(); i++) {
    //     addStroke(new Stroke(*(m_origin_strokes[i])));
    // }
}

void Group::clearLattice() {
    if (m_grid == nullptr) return;
    for (QuadPtr quad : m_grid->quads()) {
        quad.reset();
    }
    qDeleteAll(m_grid->corners());
    m_grid.reset(new Lattice(m_parentKeyframe));
}

void Group::clearLattice(int strokeId) {
    if (m_grid == nullptr) return;
    m_grid->removeStroke(strokeId, m_breakdown);
    m_grid->enforceManifoldness(this);
    m_grid->setBackwardUVDirty(true);
    setGridDirty();
}

void Group::setBreakdown(bool breakdown) {
    m_breakdown = breakdown;
    if (m_breakdown == false && m_type == PRE) {
        m_parentKeyframe->removeIntraCorrespondence(m_id);
    }
}

void Group::setGridDirty() {
    m_grid->setArapDirty();
    m_mask->setDirty();
    m_maskBackward->setDirty();
}

/**
 * Synchronize the corresponding pre group strokes (if correspondence exists)
 * And eventually the next post group lattice REF_POS (if intra-correspondence exists)
 */
void Group::syncTargetPosition(VectorKeyFrame *next) {
    if (m_type != POST) return;

    VectorKeyFrame *curKey = m_parentKeyframe;
    Group *nextPreGrp = nextPreGroup();
    Group *nextPostGrp = nextPostGroup();
    const Point::Affine &globalRigidTransform = this->globalRigidTransform(1.0f);

    if (nextPreGrp == nullptr) return;

    // move pre group strokes
    nextPreGrp->strokes().forEachPoint(next, [&](Point *point, unsigned int sId, unsigned int pId) {
        UVInfo uv = m_backwardUVs.get(sId, pId);
        point->setPos(globalRigidTransform * m_grid->getWarpedPoint(point->pos(), uv.quadKey, uv.uv, TARGET_POS));
    });

    if (nextPostGrp ==  nullptr) return;

    // move next post group lattice (ref configuration)
    nextPostGrp->lattice()->moveSrcPosTo(m_grid.get(), REF_POS, TARGET_POS);
    // move strokes with the new lattice reference position
    nextPostGrp->strokes().forEachPoint(next, [&](Point *point, unsigned int sId, unsigned int pId) {
        UVInfo uv = nextPostGrp->uvs().get(sId, pId);
        point->setPos(globalRigidTransform * nextPostGrp->lattice()->getWarpedPoint(point->pos(), uv.quadKey, uv.uv, REF_POS));
    });
    nextPostGrp->recomputeBbox();

    next->resetTrajectories();
    next->makeInbetweensDirty();
}

/**
 * Synchronize the current group with its corresponding previous pre group (if such a correspondence exists)
 * Then synchronize the corresponding previous post group
 */
void Group::syncSourcePosition(VectorKeyFrame *prev) {
    if (m_type != POST) return;

    VectorKeyFrame *curKey = m_parentKeyframe;

    Group *prevPreGrp = prevPreGroup();
    Group *prevPostGrp = prevPostGroup();

    if (prevPreGrp == nullptr || prevPostGrp == nullptr) {
        return;
    }

    prevPostGrp->lattice()->moveSrcPosTo(m_grid.get(), TARGET_POS, REF_POS);
    prevPostGrp->lattice()->setBackwardUVDirty(true);
    prevPostGrp->setGridDirty();

    prevPreGrp->strokes().forEachPoint(curKey, [&prevPostGrp](Point *point, unsigned int sId, unsigned int pId) {
        UVInfo uv = prevPostGrp->backwardUVs().get(sId, pId);
        point->setPos(prevPostGrp->lattice()->getWarpedPoint(point->pos(), uv.quadKey, uv.uv, TARGET_POS));
    });

    prev->makeInbetweensDirty();
}

void Group::syncSourcePosition() {
    if (m_type != POST) return;

    VectorKeyFrame *curKey = m_parentKeyframe;

    m_drawingPartials.firstPartial().strokes().forEachPoint(curKey, [&](Point *point, unsigned int sId, unsigned int pId) {
        UVInfo uv = m_forwardUVs.get(sId, pId);
        point->setPos(m_grid->getWarpedPoint(point->pos(), uv.quadKey, uv.uv, REF_POS));
    });

    // TODO traj?

    curKey->makeInbetweensDirty();
}

// ! deprecated
void Group::resetInterStrokes() {
    for (std::vector<Stroke *> svec : m_inter_strokes) {
        qDeleteAll(svec);
    }
    m_inter_strokes.clear();
    std::vector<Stroke *> svec;
    if (nbOriginStrokes() > 0) {
        for (Stroke *s : originStrokes()) {
            // svec.push_back(new Stroke(*s));
        }
    } else {
        // for (Stroke *s : strokes()) {
        //     svec.push_back(new Stroke(*s));
        // }
        // TODO: same as initOriginStrokes
    }
    m_inter_strokes.push_back(svec);
}

void Group::resetCanStrokes() {
    for (std::vector<Stroke *> svec : m_candidate_strokes) {
        qDeleteAll(svec);
    }
    m_candidate_strokes.clear();
}

Stroke *Group::stroke(int id) const { return m_parentKeyframe->stroke(id); }