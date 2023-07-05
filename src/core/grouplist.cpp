/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "grouplist.h"
#include "group.h"

GroupList::GroupList(GroupType type, VectorKeyFrame *parentKeyFrame) : m_gIdx(0), m_lastIdx(-1), m_type(type), m_parentKeyFrame(parentKeyFrame) {
    if (m_type == POST) {
        add(new Group(m_parentKeyFrame, QColor(Qt::black), MAIN));
    }
}

GroupList::~GroupList() {
    for (auto it = cbegin(); it != cend(); ++it) {
        delete (*it);
    }
}

// If forceAdd is set to false then no group will be added if the last group is empty
Group *GroupList::add(bool forceAdd) {
    if (!empty() && lastGroup() != nullptr && lastGroup()->size() == 0 && !forceAdd) return nullptr;
    if (!empty() && lastGroup()) lastGroup()->update();
    Group *group = new Group(m_parentKeyFrame, m_type);
    insert(group->id(), group);
    m_lastIdx = group->id();
    m_gIdx++;
    return group;
}

Group *GroupList::add(QColor color) {
    if (!empty() && lastGroup()) lastGroup()->update();
    Group *group = new Group(m_parentKeyFrame, color, m_type);
    insert(group->id(), group);
    m_lastIdx = group->id();
    m_gIdx++;
    return group;
}

Group *GroupList::add(Group *group, bool replace) {
    if (group == nullptr) return nullptr;

    // find if a group already has the same id
    if (contains(group->id())) {
        if (!replace) {
            qWarning() << "A group with the same id already exists in this keyframe (id=" << group->id() << ")";
            return value(group->id());
        }
        delete value(group->id());
    }

    insert(group->id(), group);
    m_lastIdx = group->id();
    m_gIdx++;
    return group;
}

Group *GroupList::removeGroup(int id) {
    if (id == -1 || !contains(id)) return nullptr;
    Group *group = value(id);
    remove(id);
    return group;
}

bool GroupList::containsStroke(unsigned int strokeId) {
    for (auto it = constBegin(); it != constEnd(); ++it) {
        if (it.value()->strokes().contains(strokeId)) return true;
    }
    return false;
}
