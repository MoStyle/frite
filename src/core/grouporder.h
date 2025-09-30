/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __GROUPORDER_H__
#define __GROUPORDER_H__

#include <vector>
#include <QDomElement>

class VectorKeyFrame;

class GroupOrder {
public:
    GroupOrder(VectorKeyFrame *parentKeyFrame);
    GroupOrder(const GroupOrder &other);
    virtual ~GroupOrder();

    const std::vector<std::vector<int>> &order() const { return m_order; }
    std::vector<std::vector<int>> &order() { return m_order; }

    void add(int groupId);
    void add(int groupId, int depth);
    void remove(int groupId);
    int setAOnTopOfB(int groupIdA, int groupIdB);
    int setAUnderB(int groupIdA, int groupIdB);
    void sameDepth(int groupIdA, int groupIdB);
    void reset();
    void setParentKeyFrame(VectorKeyFrame *keyframe);

    int depthOf(int groupId); 
    int nbDepths() const { return m_order.size(); }
    bool sameOrder(const GroupOrder &other) const;

    void load(const QDomElement &el);
    void save(QDomElement &el) const;

    void debug() const;

private:
    std::vector<std::vector<int>> m_order; // From closest to farthest depths (front-to-back)
    VectorKeyFrame *m_parentKeyFrame;
};

#endif // __GROUPORDER_H__