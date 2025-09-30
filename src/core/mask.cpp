/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "mask.h"

#include "vectorkeyframe.h"
#include "group.h"
#include "lattice.h"
#include "utils/stopwatch.h"
#include "dialsandknobs.h"

#include <tesselator.h>
#include <QOpenGLShaderProgram>

static dkBool k_project("Options->Mask->Project outline", true);
static dkBool k_smooth("Options->Mask->Smooth outline", true);

Mask::Mask(Group *group, bool forwardMask) : m_group(group), m_vbo(QOpenGLBuffer::VertexBuffer), m_ebo(QOpenGLBuffer::IndexBuffer), m_bufferCreated(false), m_bufferDestroyed(false), m_bufferDirty(true), m_forwardMask(forwardMask) {
    m_tessellator = tessNewTess(nullptr);
    m_dirty = true;
}

Mask::~Mask() {
    tessDeleteTess(m_tessellator);
}

/**
 * Creates a coarse mask from the boundary vertices of the lattice
 * TODO: only consider the exterior boundary! maybe start with for example the top-most quad and use the top left/right corner
 */
void Mask::computeOutline() {
    StopWatch ss("COMPUTE OUTLINE");

    StopWatch s("Outline extraction");

    const PosTypeIndex posType = m_forwardMask ? REF_POS : TARGET_POS;

    // Create a copy of the group with empty quads removed (except for AA quads)
    m_grid.reset(new Lattice(*m_group->lattice()));
    for (auto it = m_grid->quads().begin(); it != m_grid->quads().end(); ++it) {
        it.value()->setForwardStrokes(m_group->lattice()->quad(it.key())->forwardStrokes());
        it.value()->setBackwardStrokes(m_group->lattice()->quad(it.key())->backwardStrokes());
        it.value()->computeCentroid(posType);
    } 

    // Keep only quads that contain forward or backward strokes (depending on the direction), then make sure the grid is still manifold
    if (m_forwardMask) {
        m_grid->deleteQuadsPredicate([&](QuadPtr q) { return q->nbForwardStrokes() == 0 && !q->isPivot(); }); // TODO remove pivot for backwards quad
        m_grid->enforceManifoldness(m_group);
    } else {
        m_grid->deleteQuadsPredicate([&](QuadPtr q) { return q->nbBackwardStrokes() == 0; });
        m_grid->enforceManifoldness(m_group);
    }

    m_polygon.clear();
    m_outlineVertexInfo.clear();

    // Set visited flag and find one corner on the exterior boundary of the grid
    Corner *firstCorner = m_grid->findBoundaryCorner(posType);
    if (firstCorner == nullptr) {
        qWarning() << "Error in computeOutline: could not find a first corner | #corners: " << m_grid->corners().size() << " | forward? " << m_forwardMask;
        return;
    }
    
    // Walk on the boundary to form the outline polygon
    Corner * c = firstCorner;
    do {
        c->setMiscFlag(true);
        c->setFlag(BOUNDARY, true);
        m_polygon.push_back(Clipper2Lib::PointD(c->coord(posType).x(), c->coord(posType).y()));
        m_outlineVertexInfo.push_back({c->getKey(), INT_MAX, Point::VectorType::Zero(), false});
        c = m_grid->findNextBoundaryCorner(c);
    } while (c != nullptr);
    s.stop();
    m_polygon.push_back(m_polygon.front()); // make closed
    m_outlineVertexInfo.push_back(m_outlineVertexInfo.front());

    StopWatch s2("Project outline");
    if (k_project) projectOutline();
    s2.stop();
    StopWatch s3("Smooth outline");
    if (k_smooth) smoothOutline();
    s3.stop();
    StopWatch s4("Compute outline UVs");
    computeUVs();
    s4.stop();
    tessellate();

    m_dirty = false;

    ss.stop();
}

void Mask::tessellate() {
    if (m_polygon.size() < 3) return;
    StopWatch s("Mask tessellation");
    tessSetOption(m_tessellator, TESS_CONSTRAINED_DELAUNAY_TRIANGULATION, 1);
    tessAddContour(m_tessellator, 2, m_polygon.data(), 2*sizeof(double), (int)m_polygon.size());
    if(!tessTesselate(m_tessellator, TESS_WINDING_ODD, TESS_POLYGONS, 3, 2, nullptr)) qDebug() << "Error in tessellate: cannot tessellate mask!";
    s.stop();
}

void Mask::projectOutline() {
    for (int i = 0; i < m_polygon.size() - 1; ++i) {
        Corner *corner = m_grid->corners()[m_outlineVertexInfo[i].cornerKey];
        Point::VectorType currentPos(m_polygon[i].x, m_polygon[i].y);

        // Find closest stroke vertex in neighboring quads
        Point::Scalar minDist = std::numeric_limits<Point::Scalar>::max();
        Point::VectorType projectionTarget;
        bool onlyAntialiasNeighbors = true;
        for (int q = 0; q < NUM_CORNERS; ++q) {
            QuadPtr quad = corner->quads((CornerIndex)q);
            if (quad == nullptr) continue;
            onlyAntialiasNeighbors = onlyAntialiasNeighbors & (quad->isPivot() && quad->forwardStrokes().empty()); 
            if (m_forwardMask) {
                quad->forwardStrokes().forEachPoint(m_group->getParentKeyframe(), [&](Point *point, unsigned int sId, unsigned int pId) {
                    Point::Scalar dist = (point->pos() - currentPos).norm(); 
                    if (dist < minDist) {
                        minDist = dist;
                        projectionTarget = point->pos();
                    }
                });
            } else {
                quad->backwardStrokes().forEachPoint(m_group->getParentKeyframe()->nextKeyframe(), [&](Point *point, unsigned int sId, unsigned int pId) {
                    Point::Scalar dist = (point->pos() - currentPos).norm(); 
                    if (dist < minDist) {
                        minDist = dist;
                        projectionTarget = point->pos();
                    }
                });
            }
        }

        m_outlineVertexInfo[i].antialias = onlyAntialiasNeighbors;

        if (minDist != std::numeric_limits<Point::Scalar>::max()) {
            m_polygon[i].x = projectionTarget.x();
            m_polygon[i].y = projectionTarget.y();
        }
    }
    m_polygon.back() = m_polygon.front();
}

void Mask::smoothOutline() {
    const PosTypeIndex posType = m_forwardMask ? REF_POS : TARGET_POS;
    int quadKey;
    QuadPtr quad;
    Point::VectorType pos;
    Clipper2Lib::PathD oldPath = m_polygon;

    auto projectToGrid = [&](Point::VectorType &pos) {
        // If the smoothed position is not in the grid, project it on the closest edge in the grid
        if (!m_grid->contains(pos, posType, quad, quadKey)) {
            pos = m_grid->projectOnEdge(pos, quadKey);
            quad = m_grid->quad(quadKey);
            // nudge the projection a little bit toward the center of the quad to make sure the projection is in the grid
            pos += (quad->centroid(posType) - pos).normalized() * 0.1; 
            return true;
        }
        return false;
    };

    for (int i = 1; i < m_polygon.size() - 1; ++i) {
        if (!m_outlineVertexInfo[i].antialias) continue;
        m_polygon[i] = (oldPath[i-1] + oldPath[i+1]) * 0.5;
        Point::VectorType pos = Point::VectorType(m_polygon[i].x, m_polygon[i].y);
        if (projectToGrid(pos)) {
            m_polygon[i].x = pos.x();
            m_polygon[i].y = pos.y();
        }
    }
    if (m_outlineVertexInfo[0].antialias) {
        m_polygon[0] = (oldPath[m_polygon.size() - 2] + oldPath[1]) * 0.5;
        Point::VectorType firstPos = Point::VectorType(m_polygon[0].x, m_polygon[0].y);
        if (projectToGrid(firstPos)) {
            m_polygon[0].x = pos.x();
            m_polygon[0].y = pos.y();
        }
        m_polygon[m_polygon.size() - 1] = m_polygon[0];
    }
}

void Mask::computeUVs() {
    const PosTypeIndex posType = m_forwardMask ? REF_POS : TARGET_POS;
    for (int i = 0; i < m_polygon.size() - 1; ++i) {
        Point::VectorType pos(m_polygon[i].x, m_polygon[i].y);
        m_outlineVertexInfo[i].uv = m_grid->getUV(pos, posType, m_outlineVertexInfo[i].quadKey);
        if (m_outlineVertexInfo[i].quadKey == INT_MAX) {
            Corner *corner = m_grid->corners()[m_outlineVertexInfo[i].cornerKey];
            for (int j = 0; j < NUM_CORNERS; ++j) {
                QuadPtr neighborQuad = corner->quads((CornerIndex)j);
                if (neighborQuad == nullptr) continue;
                neighborQuad->computeCentroid(posType);
                Point::VectorType epsDir = (neighborQuad->centroid(posType) - pos).normalized();
                m_polygon[i].x += epsDir.x();
                m_polygon[i].y += epsDir.y();
                pos = Point::VectorType(m_polygon[i].x, m_polygon[i].y);
                m_outlineVertexInfo[i].uv = m_grid->getUV(pos, posType, m_outlineVertexInfo[i].quadKey);
                break;
            }
        }
    }
    m_outlineVertexInfo.back() = m_outlineVertexInfo.front();
}

void Mask::createBuffer(QOpenGLShaderProgram *program, VectorKeyFrame *keyframe, int inbetween) {
    if (m_bufferCreated) {
        updateBuffer(keyframe, inbetween);
        return;
    }

    m_vao.create();
    m_vao.bind();

    m_vbo.create();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    m_ebo.create();
    m_ebo.bind();
    m_ebo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    // vtx
    program->enableAttributeArray(0); 
    program->setAttributeBuffer(0, GL_DOUBLE, 0, 2, 2*sizeof(GLdouble));

    m_vao.release();
    m_vbo.release();
    m_ebo.release();

    updateBuffer(keyframe, inbetween);

    m_bufferCreated = true;
    m_bufferDestroyed = false;
}

void Mask::destroyBuffer() {
    if (!m_bufferCreated) return;
    StopWatch s("Destroying mask buffers");
    m_ebo.destroy();
    m_vbo.destroy();
    m_vao.destroy();
    m_bufferDestroyed = true;
    m_bufferCreated = false;
    s.stop();
}

void Mask::updateBuffer(VectorKeyFrame *keyframe, int inbetween) {
    StopWatch s("Update mask buffer");
    const PosTypeIndex posType = m_forwardMask ? REF_POS : TARGET_POS;
    const int nel = tessGetElementCount(m_tessellator);
    const int nve = tessGetVertexCount(m_tessellator);
    const int *el = tessGetElements(m_tessellator);
    const int *map = tessGetVertexIndices(m_tessellator);
    const double *vtx = tessGetVertices(m_tessellator);

    std::vector<int> idx;
    idx.reserve(nel*3);
    for (int i = 0; i < nel*3; ++i) {
        if (el[i] != TESS_UNDEF) idx.push_back((GLuint)el[i]);
    }

    std::vector<double> vertices;
    vertices.reserve(nve*2);
    std::copy(vtx, vtx + nve * 2, std::back_inserter(vertices));

    const Inbetween &bakedInbetween = keyframe->inbetween(inbetween);
    int qk; Point::VectorType uv;
    Point::VectorType interpolatedOutlineVertex;
    for (int i = 0; i < nve; ++i) {
        if (map[i] == TESS_UNDEF) { // new vertex, cannot be mapped to a grid corner so we have to compute its UV coord on the fly
            uv = m_group->lattice()->getUV(Point::VectorType(vertices[2*i], vertices[2*i+1]), posType, qk);
            interpolatedOutlineVertex = bakedInbetween.getWarpedPoint(m_group, {qk, uv});
        } else {
            interpolatedOutlineVertex = bakedInbetween.getWarpedPoint(m_group, {m_outlineVertexInfo[map[i]].quadKey, m_outlineVertexInfo[map[i]].uv});
        }
        vertices[2*i] = interpolatedOutlineVertex.x();
        vertices[2*i+1] = interpolatedOutlineVertex.y();
    }

    m_vbo.bind(); 
    m_vbo.allocate(vertices.data(), vertices.size() * sizeof(GLdouble));
    m_vbo.release();

    m_ebo.bind(); 
    m_ebo.allocate(idx.data(), idx.size() * sizeof(GLuint));
    m_ebo.release();
    s.stop();
}