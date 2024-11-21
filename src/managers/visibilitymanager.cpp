#include "visibilitymanager.h"

#include "gridmanager.h"
#include "layermanager.h"
#include "registrationmanager.h"
#include "layoutmanager.h"
#include "canvascommands.h"
#include "arap.h"

#include <unordered_map>
#include <unordered_set>

// TODO: add macro to all qundocommands

VisibilityManager::VisibilityManager(QObject* pParent) : BaseManager(pParent) {

}

/**
 * Initialize acceleration structure
 */
void VisibilityManager::init(VectorKeyFrame *A, VectorKeyFrame *B) {
    int strideA = A->parentLayer()->stride(A->keyframeNumber());
    int strideB = B->parentLayer()->stride(B->keyframeNumber());
    m_editor->updateInbetweens(A, 0, strideA);
    m_editor->updateInbetweens(A, strideA, strideA);
    m_editor->updateInbetweens(B, 0, strideB);
    const Inbetween inbA = A->inbetween(0);
    const Inbetween inbB = B->inbetween(0);
    m_points.clear();
    m_radiusSq.clear();
    m_pointsKeys.clear();

    // Precompute KD tree
    std::vector<Point *> data;
    data.reserve(inbA.nbVertices);
    m_keys.clear();
    m_keys.reserve(inbA.nbVertices);
    for (Group *group : A->postGroups()) {
        if (group->size() == 0) continue;
        for (auto it = group->strokes().constBegin(); it != group->strokes().constEnd(); ++it) {
            Stroke *stroke = A->stroke(it.key()); // TODO: should be first inbetween
            for (const Interval &interval : it.value()) {
                for (unsigned int i = interval.from(); i <= interval.to(); ++i) {
                    data.push_back(stroke->points()[i]); 
                    m_keys.push_back(Utils::cantor(stroke->id(), i));
                }
            }
        }
    }
    treeA.make(std::move(data));

    auto occludedVertices = m_editor->layout()->getOccludedVertices(B, 0);
    data.clear();
    data.reserve(inbB.nbVertices);
    for (Group *group : B->postGroups()) {
        if (group->size() == 0) continue;
        for (auto it = group->strokes().constBegin(); it != group->strokes().constEnd(); ++it) {
            const StrokePtr &stroke = inbB.strokes[it.key()];
            for (const Interval &interval : it.value()) {
                for (unsigned int i = interval.from(); i <= interval.to(); ++i) {
                    if (occludedVertices.find(Utils::cantor(stroke->id(), i)) == occludedVertices.end() && B->visibility().value(Utils::cantor(stroke->id(), i), 0) != -2.0) { // only visible vertices
                        data.push_back(stroke->points()[i]); 
                    }
                }
            }
        }
    }
    data.shrink_to_fit();
    treeB.make(std::move(data));
}

/**
 * Find all points in A that have no match in B.
 * A point in A is matched to a point in B if they are close enough (radius depends on stroke width)
 */
void VisibilityManager::computePointsFirstPass(VectorKeyFrame *A, VectorKeyFrame *B) {
    int strideA = A->parentLayer()->stride(A->keyframeNumber());
    const Inbetween &inbetween = A->inbetween(strideA);
    auto occludedVertices = m_editor->layout()->getOccludedVertices(A, strideA);

    // First pass 
    double radSq = 10.0;
    std::vector<std::pair<size_t, Point::Scalar>> res;
    m_points.reserve(inbetween.nbVertices);
    m_pointsKeys.reserve(inbetween.nbVertices);
    m_radiusSq.reserve(inbetween.nbVertices);
    for (Group *group : A->postGroups()) {
        for (auto it = group->strokes().constBegin(); it != group->strokes().constEnd(); ++it) {
            const Stroke *stroke = inbetween.strokes[it.key()].get(); // TODo last inb?
            double rad = stroke->strokeWidth() + 2;
            radSq = rad * rad;
            for (const Interval &interval : it.value()) {
                for (unsigned int i = interval.from(); i <= interval.to(); ++i) {
                    if (occludedVertices.find(Utils::cantor(stroke->id(), i)) == occludedVertices.end() && A->visibility().value(Utils::cantor(stroke->id(), i), 0) != -2.0) {
                        Point *point = stroke->points()[i];
                        unsigned int count = treeB.kdtree->radiusSearch(&stroke->points()[i]->pos()[0], radSq, res, nanoflann::SearchParams(10));
                        if (count == 0) {
                            m_points.push_back(A->stroke(it.key())->points()[i]);
                            m_pointsKeys.push_back(Utils::cantor(stroke->id(), i));
                            m_radiusSq.insert({Utils::cantor(stroke->id(), i), radSq});
                            A->stroke(it.key())->points()[i]->setColor(QColor(Qt::darkRed));
                        }
                    }
                }
            }
        }
    }

    // TODO separate m_points in clusters

    A->updateBuffers(); // TODO: remove
}

/**
 * 
 */
bool VisibilityManager::findSources(VectorKeyFrame *A, std::vector<Point *> &sources) {
    qDebug() << "find disappearance sources";
    sources.clear();
    std::vector<std::pair<size_t, Point::Scalar>> res;
    unsigned int size = m_points.size(); // save size to avoid iterating over the added source points
    for (unsigned int i = 0; i < size; ++i) {
        unsigned int count = treeA.kdtree->radiusSearch(&m_points[i]->pos()[0], m_radiusSq[m_pointsKeys[i]] * 2.0, res, nanoflann::SearchParams(10));
        for (unsigned int j = 0; j < count; ++j) {
            if (m_radiusSq.find(m_keys[res[j].first]) == m_radiusSq.end()) {
                // this is not in m_points
                treeA.data[res[j].first]->setColor(Qt::magenta);
                sources.push_back(treeA.data[res[j].first]);
                m_pointsKeys.push_back(m_keys[res[j].first]);
                auto c = Utils::invCantor(m_pointsKeys.back());
                m_points.push_back(A->stroke(c.first)->points()[c.second]);
                m_radiusSq[m_keys[res[j].first]] = A->stroke(c.first)->strokeWidth() * A->stroke(c.first)->strokeWidth();
            }
        }
    } 

    qDebug() << "#disappearance sources: " << sources.size();
    for (Point *s : sources) {
        std::cout << "   " << s->pos().transpose() << std::endl;
    }

    return false;
}

void VisibilityManager::assignVisibilityThreshold(VectorKeyFrame *A, const std::vector<Point *> &sources) {
    if (sources.empty()) qWarning() << "Error in assignVisibilityThreshold: no source point!";
    int strideA = A->parentLayer()->stride(A->keyframeNumber());

    std::vector<Point::VectorType> curSources(sources.size());
    for (int i = 0; i < sources.size(); ++i) {
        curSources[i] = sources[i]->pos();
    }

    // Assign visibility threshold based on distance to closest source
    Point *point;
    double maxDist = 0.0, distSqToClosestSource, d;
    int closestSourceId;
    for (unsigned int i = 0; i < m_points.size(); ++i) {
        point = m_points[i];
        distSqToClosestSource = std::numeric_limits<double>::max();
        closestSourceId = -1;
        for (Point::VectorType source : curSources) {
            d = (source - point->pos()).squaredNorm();
            if (d < distSqToClosestSource) distSqToClosestSource = d;
        }
        distSqToClosestSource = sqrt(distSqToClosestSource);
        A->visibility()[m_pointsKeys[i]] = distSqToClosestSource;
        if (distSqToClosestSource > maxDist) maxDist = distSqToClosestSource;
    }

    qDebug() << "maxDist = " << maxDist;

    // Normalize
    if (maxDist > 0.0) {
        for (unsigned int i = 0; i < m_pointsKeys.size(); ++i) {
            A->visibility()[m_pointsKeys[i]] /= maxDist;
            A->visibility()[m_pointsKeys[i]] = std::clamp(-(1.0 - A->visibility()[m_pointsKeys[i]]), -1.0, -1e-8);
        }
    }
    A->updateBuffers();
}


void VisibilityManager::initAppearance(VectorKeyFrame *A, VectorKeyFrame *B) {
    int strideA = A->parentLayer()->stride(A->keyframeNumber());
    int strideB = B->parentLayer()->stride(B->keyframeNumber());
    m_editor->updateInbetweens(A, strideA, strideA);
    m_editor->updateInbetweens(B, 0, strideB);
    const Inbetween inbA = A->inbetween(strideA);
    const Inbetween inbB = B->inbetween(0);
    m_pointsAppearance.clear();
    m_radiusSqAppearance.clear();
    m_pointsKeysAppearance.clear();
    m_strokesAppearance = StrokeIntervals();

    // Precompute KD tree
    std::vector<Point *> dataB;
    dataB.reserve(inbB.nbVertices);
    m_keys.clear();
    m_keys.reserve(inbB.nbVertices);
    for (Group *group : B->postGroups()) {
        if (group->size() == 0) continue;
        for (auto it = group->strokes().constBegin(); it != group->strokes().constEnd(); ++it) {
            Stroke *stroke = B->stroke(it.key());
            for (const Interval &interval : it.value()) {
                for (unsigned int i = interval.from(); i <= interval.to(); ++i) {
                    dataB.push_back(stroke->points()[i]); 
                    m_keys.push_back(Utils::cantor(stroke->id(), i));
                }
            }
        }
    }

    treeA.make(A, strideA);
    treeB.make(std::move(dataB));
}

/**
 * Find all points in B that have no match in A.
 */
void VisibilityManager::computePointsFirstPassAppearance(VectorKeyFrame *A, VectorKeyFrame *B) {
    const Inbetween &inbetween = B->inbetween(0);

    // First pass 
    double radSq = 10.0;
    std::vector<std::pair<size_t, Point::Scalar>> res;
    m_points.reserve(inbetween.nbVertices);
    m_pointsKeys.reserve(inbetween.nbVertices);
    m_radiusSq.reserve(inbetween.nbVertices);
    for (Group *group : B->postGroups()) {
        for (auto it = group->strokes().constBegin(); it != group->strokes().constEnd(); ++it) {
            const Stroke *stroke = inbetween.strokes[it.key()].get(); // TODo last inb?
            for (const Interval &interval : it.value()) {
                int start = -1, end = -1;
                for (unsigned int i = interval.from(); i <= interval.to(); ++i) {
                    Point *point = stroke->points()[i];
                    double rad = stroke->strokeWidth() * point->pressure() + 2;
                    radSq = rad * rad;
                    unsigned int count = treeA.kdtree->radiusSearch(&stroke->points()[i]->pos()[0], radSq * 2.0, res, nanoflann::SearchParams(10));
                    if (count == 0) {
                        // TODO stroke intervals
                        if (start == -1) start = i;
                        end = i;
                        m_pointsAppearance.push_back(B->stroke(it.key())->points()[i]);
                        m_pointsKeysAppearance.push_back(Utils::cantor(stroke->id(), i));
                        m_radiusSqAppearance.insert({Utils::cantor(stroke->id(), i), radSq});
                        B->stroke(it.key())->points()[i]->setColor(QColor(2, 68, 252));
                    } else {
                        if (start != end) m_strokesAppearance[stroke->id()].append(Interval(start, end));
                        start = -1;
                        end = -1;
                    }
                }
                if (start != end) {
                    m_strokesAppearance[stroke->id()].append(Interval(start, end));
                }
            }
        }
    }

    qDebug() << "points : " << m_strokesAppearance.nbPoints() << " vs " << m_pointsAppearance.size();

    B->updateBuffers();
}

/**
 * 
 */
bool VisibilityManager::findSourcesAppearance(VectorKeyFrame *B, std::vector<Point::VectorType> &sources) {
    qDebug() << "find appearance sources " << m_pointsAppearance.size();
    sources.clear();
    std::vector<std::pair<size_t, Point::Scalar>> res;
    qDebug() << m_radiusSqAppearance.size() << "/" << m_keys.size();
    unsigned int size = m_pointsAppearance.size();
    std::vector<unsigned int> appearanceSourcesKeysArray;
    m_appearanceSourcesKeys.clear();

    for (unsigned int i = 0; i < size; ++i) {
        unsigned int count = treeB.kdtree->radiusSearch(&m_pointsAppearance[i]->pos()[0], m_radiusSqAppearance[m_pointsKeysAppearance[i]] * 2.0, res, nanoflann::SearchParams(10));
        for (unsigned int j = 0; j < count; ++j) {
            if (m_radiusSqAppearance.find(m_keys[res[j].first]) == m_radiusSqAppearance.end()) {
                // this is not in m_pointsAppearance
                auto c  = Utils::invCantor(m_keys[res[j].first]);
                treeB.data[res[j].first]->setColor(Qt::magenta);
                sources.push_back(treeB.data[res[j].first]->pos());
                m_appearanceSourcesKeys.insert(m_keys[res[j].first]);
                appearanceSourcesKeysArray.push_back(m_keys[res[j].first]);
                m_pointsKeysAppearance.push_back(m_keys[res[j].first]);
                // qDebug() << c.first << " | " << c.second;
                m_pointsAppearance.push_back(B->stroke(c.first)->points()[c.second]);
                m_radiusSqAppearance[m_keys[res[j].first]] = B->stroke(c.first)->strokeWidth() * B->stroke(c.first)->strokeWidth();
                // qDebug() << "appending " << c.second;
                m_strokesAppearance[c.first].append(Interval(c.second, c.second));
                m_strokesAppearance.debug();
                qDebug() << "-----------";
            }
        }
    }

    m_appearanceKeyToIndex.clear();
    qDebug() << "m_appearanceSourcesKeys idx: ";
    for (auto it = m_appearanceSourcesKeys.begin(); it != m_appearanceSourcesKeys.end(); ++it) {
        unsigned int idx = std::distance(appearanceSourcesKeysArray.begin(), std::find(appearanceSourcesKeysArray.begin(), appearanceSourcesKeysArray.end(), *it));
        m_appearanceKeyToIndex.insert({*it, idx});
        qDebug() << "       idx = " << idx;
    }

    // for (auto it = m_strokesAppearance.constBegin(); it != m_strokesAppearance.end(); ++it) {
    //     for (int j = 0; j < it.value().size() - 1; ++j) {
    //         for (int k = j + 1; k < it.value().size(); ++k) {
    //             qDebug() << "is " << it.value().at(j).from() << ", " << it.value().at(j).to() << " connected to " << it.value().at(k).from() << ", " << it.value().at(k).to() << " : " << (it.value().at(j).connected(it.value().at(k)));
    //         }
    //     }
    // }

    qDebug() << "m_pointsAppearance.size(): " << m_pointsAppearance.size();

    qDebug() << "#appearance sources: " << sources.size();
    for (Point::VectorType s : sources) {
        std::cout << "   " << s.transpose() << std::endl;
    }

    qDebug() << "points : " << m_strokesAppearance.nbPoints() << " vs " << m_pointsAppearance.size();

    // Remove intervals of size one
    qDebug() << "Removing single point intervals";
    std::vector<unsigned int> pointsKeyToRemove;
    QMutableHashIterator<unsigned int, Intervals> strokes(m_strokesAppearance);
    while (strokes.hasNext()) {
        strokes.next();
        QMutableListIterator<Interval> intervalIt(strokes.value());
        while (intervalIt.hasNext()) {
            Interval &inter = intervalIt.next();
            if (inter.from() == inter.to()) {
                pointsKeyToRemove.push_back(Utils::cantor(strokes.key(), inter.from()));
                intervalIt.remove();
            }
        }
        if (strokes.value().empty()) {
            strokes.remove();
        }
    }
    m_strokesAppearance.debug();
    for (unsigned int c : pointsKeyToRemove) {
        auto p = Utils::invCantor(c);
        Point *point = B->stroke(p.first)->points()[p.second];
        m_pointsAppearance.erase(std::find(m_pointsAppearance.begin(), m_pointsAppearance.end(), point));
        m_pointsKeysAppearance.erase(std::find(m_pointsKeysAppearance.begin(), m_pointsKeysAppearance.end(), c));
        m_radiusSqAppearance.erase(c);
        appearanceSourcesKeysArray.erase(std::find(appearanceSourcesKeysArray.begin(), appearanceSourcesKeysArray.end(), c));
        m_appearanceSourcesKeys.erase(c);
        sources.erase(std::find(sources.begin(), sources.end(), point->pos()));
    }

    B->updateBuffers();

    return false;
}

/**
 * 
 */
void VisibilityManager::addGroupsOrBake(VectorKeyFrame *A, VectorKeyFrame *B, std::vector<Point::VectorType> &sources, std::vector<int> &sourcesGroupsId) {
    qDebug() << "addGroupsOrBake";

    m_appearingPointsCluster.clear();
    m_appearingPointsKeys.clear();
    m_clusterIdx = 0;

    sourcesGroupsId = std::vector<int>(sources.size());
    std::unordered_map<unsigned int, unsigned int> sourcesKeyToKey; // (key in B, key in A)

    std::set<int> nonNewGroupA; // set for constant search
    for (Group *group : A->postGroups()) nonNewGroupA.insert(group->id());

    // Bake and remove stroke intervals that are fully inside a group in A
    QMutableHashIterator<unsigned int, Intervals> it(m_strokesAppearance);
    while (it.hasNext()) {
        it.next();
        Stroke *stroke = B->stroke(it.key());
        std::vector<int> indicesToRemove;
        QMutableListIterator<Interval> itIntervals(it.value());
        while (itIntervals.hasNext()) {
            const Interval &interval = itIntervals.next();
            bool found = false;
            // Can the stroke be fully baked inside a group in A (checking from top-to-bottom)
            for (const std::vector<int> &groups : A->orderPartials().firstPartial().groupOrder().order()) {
                for (int groupId : groups) {
                    Group *group = A->postGroups().fromId(groupId);
                    qDebug() << "Testing " << stroke->id() << " [" << interval.from() << ", " << interval.to() << "]";
                    if (group->lattice() != nullptr && group->lattice()->contains(stroke, interval.from(), interval.to(), TARGET_POS, true)) {
                        // Bake new stroke in A
                        qDebug() << "Baking " << stroke->id() << " [" << interval.from() << ", " << interval.to() << "]";
                        unsigned int newId = A->pullMaxStrokeIdx();
                        StrokePtr copiedStroke = std::make_shared<Stroke>(*stroke, newId, interval.from(), interval.to());
                        DrawCommand drawCommand(m_editor, A->parentLayerOrder(), A->keyframeNumber(), copiedStroke, Group::ERROR_ID, false); // we don't wan't to undo this because its handled by ComputeVisibilityCommand
                        drawCommand.redo();
                        Stroke *newStroke = A->stroke(newId);
                        Interval newInter(0, newStroke->size() - 1);
                        group->addStroke(newId);
                        m_editor->grid()->bakeStrokeInGrid(group->lattice(), newStroke, 0, newStroke->size() - 1, TARGET_POS, true);
                        group->lattice()->bakeForwardUV(newStroke, newInter, group->uvs(), TARGET_POS);
                        for (int i = 0; i < newStroke->size(); ++i) {
                            m_appearingPointsKeys.push_back({Utils::cantor(newId, i), group->stroke(newId)->points()[i]});
                            m_appearingPointsCluster.push_back(m_clusterIdx);
                        }
                        for (int i = interval.from(); i <= interval.to(); ++i) { 
                            if (m_appearanceSourcesKeys.find(Utils::cantor(it.key(), i)) != m_appearanceSourcesKeys.end()) {
                                sourcesKeyToKey.insert({Utils::cantor(it.key(), i), Utils::cantor(newId, i - interval.from())});
                            }
                        }
                        ++m_clusterIdx;
                        itIntervals.remove();
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
        }
        if (it.value().empty()) it.remove();
    }

    qDebug() << "m_strokesAppearance.size(): " << m_strokesAppearance.size();
    if (m_strokesAppearance.empty()) return;

    // Add new group and put all the remaining strokes inside
    AddGroupCommand addGroupCommand(m_editor, A->parentLayerOrder(), A->keyframeNumber()); // we don't wan't to undo this because its handled by ComputeVisibilityCommand
    addGroupCommand.redo();
    Group *allStrokesGroup = A->postGroups().lastGroup();
    for (auto it = m_strokesAppearance.begin(); it != m_strokesAppearance.end(); ++it) {
        Stroke *stroke = B->stroke(it.key());
        for (const Interval &interval : it.value()) {
            unsigned int newId = A->pullMaxStrokeIdx();
            StrokePtr copiedStroke = std::make_shared<Stroke>(*stroke, newId, interval.from(), interval.to());
            DrawCommand drawCommand(m_editor, A->parentLayerOrder(), A->keyframeNumber(), copiedStroke, allStrokesGroup->id(), false); // we don't wan't to undo this because its handled by ComputeVisibilityCommand
            drawCommand.redo();
            Stroke *newStroke = A->stroke(newId);
            for (int i = interval.from(); i <= interval.to(); ++i) { 
                if (m_appearanceSourcesKeys.find(Utils::cantor(it.key(), i)) != m_appearanceSourcesKeys.end()) {
                    sourcesKeyToKey.insert({Utils::cantor(it.key(), i), Utils::cantor(newId, i - interval.from())});
                }
            }
        }
    }

    // Split the new group in multiple connected components if needed
    int idxBefore = m_editor->undoStack()->index();
    auto newGroups = m_editor->splitGridIntoSingleConnectedComponent();
    int idxAfter = m_editor->undoStack()->index();
    // sorry....
    for (int idxCur = idxAfter - 1; idxCur >= idxBefore; --idxCur) {
        auto* cmd = const_cast<QUndoCommand*>(m_editor->undoStack()->command(idxCur)); 
        cmd->setObsolete(true);
    }
    m_editor->undoStack()->setIndex(idxBefore);

    if (A->postGroups().fromId(allStrokesGroup->id()) != nullptr) newGroups.insert(allStrokesGroup->id());
    std::set<int> isolatedNewGroups, extensionFailGroup, mergedNewGroups;

    // If a new group intersects a non-new group in A, try to merge them
    for (int newGroupId : newGroups) {
        Group *newGroup = A->postGroups().fromId(newGroupId);
        for (const std::vector<int> &groups : A->orderPartials().firstPartial().groupOrder().order()) {
            for (int groupId : groups) {
                if (std::find(newGroups.begin(), newGroups.end(), groupId) != newGroups.end()) continue; // It's a new group, skip it
                Group *group = A->postGroups().fromId(groupId);
                if (group->lattice()->intersects(A, newGroup->strokes(), TARGET_POS)) {
                    qDebug() << "New group " << newGroupId << " intersects " << groupId << " -> merging";
                    StrokeIntervals added;
                    StrokeIntervals notAdded;
                    if (m_editor->grid()->expandTargetGridToFitStroke(group, newGroup->strokes(), added, notAdded)) {
                        RemoveGroupCommand removeGroupCommand(m_editor, A->parentLayerOrder(), A->keyframeNumber(), newGroupId, POST); // remove group but not strokes
                        removeGroupCommand.redo();
                        // Bake in the expanded group
                        for (auto it = added.begin(); it != added.end(); ++it) {
                            group->addStroke(it.key(), it.value());
                            for (Interval &interval : it.value()) {
                                m_editor->grid()->bakeStrokeInGridWithConnectivityCheck(group->lattice(), A->stroke(it.key()), interval.from(), interval.to(), TARGET_POS, true);
                                group->lattice()->bakeForwardUVConnectivityCheck(A->stroke(it.key()), interval, group->uvs(), TARGET_POS);
                            }
                        }
                        mergedNewGroups.insert(group->id());
                        added.forEachPoint(A, [&](Point *p, unsigned int sid, unsigned int pid) { 
                            m_appearingPointsKeys.push_back({Utils::cantor(sid, pid), group->stroke(sid)->points()[pid]}); 
                            m_appearingPointsCluster.push_back(m_clusterIdx);
                        });
                        ++m_clusterIdx;
                    } else {
                        extensionFailGroup.insert(newGroup->id());
                    }
                } else {
                    isolatedNewGroups.insert(newGroup->id());
                }
            }
        }
    }

    qDebug() << "#mergedNewGroups: " << mergedNewGroups.size();
    qDebug() << "#extensionFailGroup: " << extensionFailGroup.size();
    qDebug() << "#isolatedNewGroups: " << isolatedNewGroups.size();

    // Add trajectory constraints at diffusion sources if possible
    std::set<int> pinnedNewGroups;
    std::unordered_map<int, std::pair<int, Point::VectorType>> map; // new group pinned quad key -> prev group corresponding (quad key, uv) 
    for (auto it = sourcesKeyToKey.begin(); it != sourcesKeyToKey.end(); ++it) {
        unsigned int iSource = m_appearanceKeyToIndex[it->first];
        Point::VectorType source = sources[iSource];
        bool sourceInMergedGroup = false, sourceInExtensionFailedGroup = false;

        auto pointInfo = Utils::invCantor(it->second);
        Point *sourceInA = A->stroke(pointInfo.first)->points()[pointInfo.second];
        Group *sourceGroup = A->postGroups().fromId(sourceInA->groupId());

        Q_ASSERT_X(sourceGroup != nullptr, "addGroupsOrBake add trajectory constraints", "cannot find the new group the source point belongs to");

        sourcesGroupsId[iSource] = sourceInA->groupId();

        sourceInExtensionFailedGroup = extensionFailGroup.find(sourceInA->groupId()) != extensionFailGroup.end();

        qDebug() << "in merged? " << sourceInMergedGroup << " | in failed? " << sourceInExtensionFailedGroup;

        if (sourceInExtensionFailedGroup) {
            // Find potential intersection with non-new group in A
            bool found = false;
            int k;
            QuadPtr q;
            Group *nonNewGroup = nullptr;
            for (const std::vector<int> &groups : A->orderPartials().firstPartial().groupOrder().order()) {
                for (int groupId : groups) {
                    if (nonNewGroupA.find(groupId) == nonNewGroupA.end()) continue;
                    if (A->postGroups().fromId(groupId)->lattice()->contains(source, TARGET_POS, q, k)) {
                        found = true;
                        nonNewGroup = A->postGroups().fromId(groupId);
                        break;
                    }
                }
                if (found) break;
            }

            if (found) {
                // TODO we should be able to add multiple constraints in a quad
                Point::VectorType uv = nonNewGroup->lattice()->getUV(source, TARGET_POS, q);
                Point::VectorType targetPos = nonNewGroup->lattice()->getWarpedPoint(Point::VectorType::Zero(), k, uv, REF_POS);
                qDebug() << "pinning new group " << sourceGroup << " to " << nonNewGroup->id();
                int keySource;
                Point::VectorType uvSource = sourceGroup->lattice()->getUV(source, TARGET_POS, keySource);
                Q_ASSERT_X(keySource != INT_MAX, "addGroupsOrBake add trajectory constraints", "cannot find the quad the source point belongs to");
                sourceGroup->lattice()->quad(keySource)->pin(uvSource, targetPos);
                pinnedNewGroups.insert(sourceGroup->id());
                sources[iSource] = targetPos;
            }
        }
    //     // TODO: use the "direct matching" algo the find the REF_POS of the new group
    //     // TODO: do traj and matching in reverse and then swap everything (pos, trajectories start and ends, ...) at the end
    //     // auto traj = std::make_shared<Trajectory>(A, newGroup, )
    //     // m_editor->undoStack()->push(new AddTrajectoryConstraintCommand(m_editor, A->parentLayerOrder(), A->keyframeNumber(), ));
    }


    qDebug() << "#pinnedNewGroups: " << pinnedNewGroups.size();

    // Reverse matching
    for (int groupId : pinnedNewGroups) {
        Group *group = A->postGroups().fromId(groupId);
        m_editor->registration()->applyOptimalRigidTransformBasedOnPinnedQuads(group);
        group->lattice()->displacePinsQuads(TARGET_POS);
        // qDebug() << "regularizing " << group->id();
        int iterations = Arap::regularizeLattice(*group->lattice(), REF_POS, TARGET_POS, 5000, true, true, false);
        // qDebug() << "#iterations: " << iterations;
        group->lattice()->displacePinsQuads(TARGET_POS);
        group->lattice()->copyPositions(group->lattice(), TARGET_POS, INTERP_POS);
        group->lattice()->copyPositions(group->lattice(), REF_POS, TARGET_POS);
        group->lattice()->copyPositions(group->lattice(), INTERP_POS, REF_POS);

        for (QuadPtr q : group->lattice()->quads()) {
            if (q->isPinned()) {
                auto traj = std::make_shared<Trajectory>(A, group, UVInfo{q->key(), q->pinUV()});
                AddTrajectoryConstraintCommand addTrajConstraintCommand(m_editor, A->parentLayerOrder(), A->keyframeNumber(), traj);
                addTrajConstraintCommand.redo();
            }
            q->unpin();
        }

        group->setGridDirty();
        group->syncSourcePosition();
    }

    // Add all new points
    for (int id : extensionFailGroup) {
        A->postGroups().fromId(id)->strokes().forEachPoint(A, [&](Point *p, unsigned int sid, unsigned int pid) { 
            m_appearingPointsKeys.push_back({Utils::cantor(sid, pid), p}); 
            m_appearingPointsCluster.push_back(m_clusterIdx);
        });
        ++m_clusterIdx;
    }
}

void VisibilityManager::assignVisibilityThresholdAppearance(VectorKeyFrame *A, const std::vector<Point::VectorType> &sources, const std::vector<int> sourcesGroupsId) {
    if (sources.empty()) qWarning() << "Error in assignVisibilityThreshold: no source point!";

    qDebug() << "cur sources size: " << sources.size();
    qDebug() << "m_appearingPointsKeys size: " << m_appearingPointsKeys.size();

    const unsigned int clusters = std::max(m_clusterIdx, 0);
    std::vector<double> clusterMaxDist(clusters);
    for (int i = 0; i < clusters; ++i) clusterMaxDist[i] = 0.0;

    for (int i = 0; i < sources.size(); ++i) {
        std::cout << "source: " << sources[i].transpose() << "   " << sourcesGroupsId[i] << std::endl;
    }

    // Assign visibility threshold based on distance to closest source
    Point *point;
    double distSqToClosestSource, d;
    int closestSourceId, count;
    for (unsigned int i = 0; i < m_appearingPointsKeys.size(); ++i) {
        point = m_appearingPointsKeys[i].second;
        distSqToClosestSource = std::numeric_limits<double>::max();
        closestSourceId = -1;
        count = 0;
        for (unsigned int j = 0; j < sources.size(); ++j) {
            if (sourcesGroupsId[j] != point->groupId()) continue;
            ++count;
            Point::VectorType source = sources[j];
            d = (source - point->pos()).squaredNorm();
            if (d < distSqToClosestSource) distSqToClosestSource = d;
        }
        if (count == 0) continue;
        distSqToClosestSource = sqrt(distSqToClosestSource);
        A->visibility()[m_appearingPointsKeys[i].first] = distSqToClosestSource;
        if (distSqToClosestSource > clusterMaxDist[m_appearingPointsCluster[i]]) clusterMaxDist[m_appearingPointsCluster[i]] = distSqToClosestSource;
    }

    // Normalize
    for (unsigned int i = 0; i < m_appearingPointsKeys.size(); ++i) {
        if (!A->visibility().contains(m_appearingPointsKeys[i].first) || clusterMaxDist[m_appearingPointsCluster[i]] == 0) continue; // skip points that are in a cluster with no sources
        A->visibility()[m_appearingPointsKeys[i].first] /= clusterMaxDist[m_appearingPointsCluster[i]];
        A->visibility()[m_appearingPointsKeys[i].first] = std::clamp((A->visibility()[m_appearingPointsKeys[i].first]), 0.0, 1.0);
    }
    A->updateBuffers();
}