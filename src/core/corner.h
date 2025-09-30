/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

 #ifndef CORNER_H
#define CORNER_H

#include "Eigen/Core"

#include "quad.h"

#include <bitset>

typedef enum { TOP_LEFT = 0, TOP_RIGHT, BOTTOM_RIGHT, BOTTOM_LEFT, NUM_CORNERS } CornerIndex;
typedef enum { TOP_EDGE = 0, RIGHT_EDGE, BOTTOM_EDGE, LEFT_EDGE, NUM_EDGES } EdgeIndex;

typedef enum { 
    MOVABLE = 0,    // Whether the corner is movable (by user interactions)
    BOUNDARY,       // Whether the corner is on the boundary of the grid
    UNUSED6, 
    UNUSED7, 
    UNUSED8, 
    UNUSED9, 
    UNUSED10,   
    MISC_CORNER     // Used for storing temporary states, may be overwritten, only store temporary states!
} CornerFlags;

class Quad;

/**
 * Represents a lattice corner. May be adjacent to up to 4 quads.
 *
 * Corners have 4 coordinates (PosTypeIndex) representing the source and target configuration of a lattice as well as intermediate and deformed configuration
 * used for various ARAP interpolation and deformation respectively.
 */
class Corner {
   public:
    Corner(Point::VectorType c = Point::VectorType::Zero()) : m_nbQuads(0), m_key(-1) {
        for (int i = 0; i < NUM_COORDS; i++) m_coord[i] = c;
        for (int i = 0; i < NUM_CORNERS; i++) m_quads[i] = nullptr;
        setDeformable(true);
    }

    ~Corner() {}

    void setKey(int k) { m_key = k; }
    int getKey() { return m_key; }

    // Getters and setters for retrocomp
    bool isDeformable() const { return m_flags.test(MOVABLE); }
    void setDeformable(bool b) { m_flags.set(MOVABLE, b); }
    bool miscFlag() const { return m_flags.test(MISC_CORNER); }
    void setMiscFlag(bool flag) { m_flags.set(MISC_CORNER, flag); }
    bool flag(int flag) const { return m_flags.test(flag); }
    void setFlag(int flag, bool b) { m_flags.set(flag, b); }
    void setFlags(const std::bitset<8> &flags) { m_flags = flags;}
    const std::bitset<8> &flags() const { return m_flags; }
    

    inline Point::VectorType coord(PosTypeIndex i) const { return m_coord[i]; }
    inline Point::VectorType& coord(PosTypeIndex i) { return m_coord[i]; }

    QuadPtr& quads(CornerIndex i) { return m_quads[i]; }

    int nbQuads() const { return m_nbQuads; }
    void setNbQuads(int nb) { m_nbQuads = nb; }
    void incrNbQuads() { m_nbQuads++; }

   private:
    QuadPtr m_quads[NUM_CORNERS];           // adjacent quads (may be nullptr)
    int m_nbQuads;                          // nb of adjacent quads
    std::bitset<8> m_flags;                 // store corner properties
    Point::VectorType m_coord[NUM_COORDS];  // coordinates of the corner (see class description)
    int m_key;                              // corner id
};

#endif  // CORNER_H
