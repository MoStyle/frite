/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef REGISTRATIONMANAGER_H
#define REGISTRATIONMANAGER_H

#include <QTransform>
#include "point.h"
#include "basemanager.h"
#include "corner.h"
#include <nanoflann.hpp>
#include "nanoflann_datasetadaptor.h"

using namespace Eigen;

class VectorKeyFrame;
class Group;

class RegistrationManager : public BaseManager
{
    Q_OBJECT

public :
    typedef nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<Point::Scalar, DatasetAdaptorPoint>, DatasetAdaptorPoint, 2, size_t> KDTree;

    RegistrationManager(QObject* pParent);

    void preRegistration(Group *source, PosTypeIndex type);
    void preRegistration(const QHash<int, Group *> &groups, PosTypeIndex type);
    void registration(Group *source, PosTypeIndex type, PosTypeIndex regularizationSource, bool usePreRegistration);
    void registration(Group *source, PosTypeIndex type, PosTypeIndex regularizationSource, bool usePreRegistration, int registrationIt, int regularizationIt);

    void setRegistrationTarget(VectorKeyFrame *targetKey);
    void setRegistrationTarget(VectorKeyFrame *targetKey, StrokeIntervals &targetStrokes);
    void setRegistrationTarget(VectorKeyFrame *targetKey, const std::vector<Point *> &targetPos);
    void clearRegistrationTarget();
    bool registrationTargetEmpty() const { return m_registrationTargetPoints.empty(); }

protected:
    void alignCenterOfMass(Group *source);
    void rigidCPD(const QHash<int, Group *> &groups);
    void pushPhaseWithoutCoverage(Group *source);
    void pushPhaseWithCoverage(Group *source);
    void resetKDTree();
private:
    VectorKeyFrame *m_registrationTargetKey;
    std::vector<Point *> m_registrationTargetPoints;
    Point::VectorType m_registrationTargetPointsCM; 
    std::unique_ptr<DatasetAdaptorPoint> m_registrationDataset;
    std::unique_ptr<KDTree> m_registrationKDTree;
};
#endif // REGISTRATIONMANAGER_H
