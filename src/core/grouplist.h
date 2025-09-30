/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __GROUPLIST_H__
#define __GROUPLIST_H__

#include <vector>

#include <QColor>

#include "group.h"

class VectorKeyFrame;

// If m_type is POST then the GroupList will always have at least one group
// If m_type is PRE then the list can be empty
class GroupList : public QMap<int, Group *> {
public:
    GroupList(GroupType type, VectorKeyFrame *parentKeyFrame);
    virtual ~GroupList();

    Group *add(bool forceAdd = false);
    Group *add(QColor color);
    Group *add(Group *group, bool replace = false);

    Group *removeGroup(int id);

    inline Group *fromId(int id) const { return value(id); }
    inline VectorKeyFrame *parentKeyframe() const { return m_parentKeyFrame; };
    bool containsStroke(unsigned int strokeId);

    inline Group *lastGroup() const { return m_lastIdx == -2 ? nullptr : value(m_lastIdx); }
    inline size_t nbGroups() const { return size(); }
    inline int curIdx() const { return size() > 0 ? lastKey() + 1 : 0; };
    inline GroupType type() const { return m_type; }

private:
    int m_gIdx, m_lastIdx;
    GroupType m_type;
    VectorKeyFrame *m_parentKeyFrame;
};

#endif // __GROUPLIST_H__