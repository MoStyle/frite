/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef CORNER_H
#define CORNER_H

#include "Eigen/Core"

#include "quad.h"

typedef enum { TOP_LEFT = 0, TOP_RIGHT, BOTTOM_RIGHT, BOTTOM_LEFT, NUM_CORNERS } CornerIndex;

class Quad;

/**
 * Represents a lattice corner. May be adjacent to up to 4 quads.
 *
 * Corners have 4 coordinates (PosTypeIndex) representing the source and target configuration of a lattice as well as intermediate and deformed configuration
 * used for various ARAP interpolation and deformation respectively.
 */
class Corner {
   public:
    Corner(Point::VectorType c = Point::VectorType::Zero()) : m_nbQuads(0), m_deformable(false), m_flag(false), m_key(-1) {
        for (int i = 0; i < NUM_COORDS; i++) m_coord[i] = c;
        for (int i = 0; i < NUM_CORNERS; i++) m_quads[i] = nullptr;
    }

    ~Corner() {}

    void setKey(int k) { m_key = k; }
    int getKey() { return m_key; }

    bool isDeformable() const { return m_deformable; }
    void setDeformable(bool b) { m_deformable = b; }

    bool flag() const { return m_flag; }
    void setFlag(bool flag) { m_flag = flag; }

    inline Point::VectorType coord(PosTypeIndex i) const { return m_coord[i]; }
    inline Point::VectorType& coord(PosTypeIndex i) { return m_coord[i]; }

    QuadPtr& quads(CornerIndex i) { return m_quads[i]; }

    int nbQuads() const { return m_nbQuads; }
    void setNbQuads(int nb) { m_nbQuads = nb; }
    void incrNbQuads() { m_nbQuads++; }

   private:
    QuadPtr m_quads[NUM_CORNERS];           // adjacent quads (may be nullptr)
    int m_nbQuads;                          // nb of adjacent quads 
    bool m_deformable;                      // if false the vertex cannot be moved 
    bool m_flag;                            // temporary flag for misc. algorithms
    Point::VectorType m_coord[NUM_COORDS];  // coordinates of the corner (see class description)
    int m_key;                              // corner id
};

#endif  // CORNER_H
