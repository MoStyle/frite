/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "test.h"

#include "vectorkeyframe.h"
#include "group.h"
#include "lattice.h"

void Test::sampleGridCornerTrajectory(Group *group, Lattice *grid, Corner *corner, unsigned int samples, std::vector<Point::VectorType> &outTrajectoryPoly) {
    outTrajectoryPoly.clear();
    outTrajectoryPoly.reserve(samples + 2);
    VectorKeyFrame *key = group->getParentKeyframe();
    double step = 1.0 / (double)(samples + 1);
    double t = 0.0;
    double cumLength = 0.0;
    while (t <= 1.0) {
        grid->interpolateARAP(t, group->spacingAlpha(t), key->rigidTransform((float)t));
        outTrajectoryPoly.push_back(corner->coord(INTERP_POS));
        t += step;
    }
}

Point::Scalar Test::evalCornerTrajectoryArcLength(Group *group, Lattice *grid, Corner *corner, std::vector<double> &outDiffs) {
    std::vector<Point::VectorType> trajPoly;
    const unsigned int SAMPLES = 100;
    sampleGridCornerTrajectory(group, grid, corner, SAMPLES, trajPoly);

    std::vector<double> cumLength(trajPoly.size());
    for (int i = 0; i < trajPoly.size(); ++i) {
        if (i == 0)
            cumLength[0] = 0.0;
        else {
            Point::VectorType diff = trajPoly[i] - trajPoly[i - 1];
            cumLength[i] = cumLength[i - 1] + diff.norm();
        }
    }

    VectorKeyFrame *key = group->getParentKeyframe();
    outDiffs.resize(trajPoly.size());
    for (int i = 0; i < trajPoly.size(); ++i) {
        double t = (double)i / (double)(trajPoly.size() - 1);
        key->spacing()->frameChanged(t);
        double t_key = key->spacing()->get();
        // std::cout << "t_linear=" << t << "  |  t_group=" << group->spacingAlpha(t) << "  |  t_key=" << t_key << "  |  norm_arclen=" << (cumLength[i] / cumLength.back()) << std::endl;
        // std::cout << "group_diff=" << std::abs((cumLength[i] / cumLength.back()) - group->spacingAlpha(t)) << std::endl;
        outDiffs[i] = std::abs((cumLength[i] / cumLength.back()) - t_key /* group->spacingAlpha(t) */);
    }

    return 0.0;
}

void Test::test(Group *group, Lattice *grid) {
    std::vector<std::vector<double>> diffs;

    VectorKeyFrame *key = group->getParentKeyframe();

    diffs.resize(grid->corners().size());
    int i = 0;
    for (Corner *corner : grid->corners()) {
        Test::evalCornerTrajectoryArcLength(group, grid, corner, diffs[i]);
        ++i;
    }
    int diffSize = diffs[0].size();
    for (int i = 0; i < diffSize; ++i) {
        // double t_group = group->spacingAlpha(((double)i) / ((double)diffSize-1));
        key->spacing()->frameChanged(((double)i) / ((double)diffSize - 1));
        double t_key = key->spacing()->get();
        std::cout << "* T= " << t_key << ": " << std::endl;
        for (int j = 0; j < diffs.size(); ++j) {
            std::cout << "     " << diffs[j][i] << std::endl;
        }
    }
}