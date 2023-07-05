/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __TEST_H__
#define __TEST_H__

#include <vector>
#include <QTransform>

#include "point.h"

class VectorKeyFrame;
class Group;
class Lattice;
class Corner;

class Test {
   public:
    static void sampleGridCornerTrajectory(Group *group, Lattice *grid, Corner *corner, unsigned int samples, std::vector<Point::VectorType> &outTrajectoryPoly);
    static Point::Scalar evalCornerTrajectoryArcLength(Group *group, Lattice *grid, Corner *corner, std::vector<double> &outDiffs);
    static void test(Group *group, Lattice *grid);
};

#endif  // __TEST_H__