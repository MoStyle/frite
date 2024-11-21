#include "grouporder.h"
#include "group.h"

#include <vector>
#include <algorithm>

GroupOrder::GroupOrder(VectorKeyFrame *parentKeyFrame) : m_parentKeyFrame(parentKeyFrame) {
    m_order.emplace_back();
}

GroupOrder::GroupOrder(const GroupOrder &other) 
    : m_order(other.m_order),
      m_parentKeyFrame(other.m_parentKeyFrame) {
        
}

GroupOrder::~GroupOrder() {

}

/**
 * Add the group id at the closest depth
 */
void GroupOrder::add(int groupId) {
    m_order.front().push_back(groupId);
}

/**
 * Add the group id at the given depth
 * If the given is one more than last depth, a new depth is added
 */
void GroupOrder::add(int groupId, int depth) {
    if (depth < 0 || depth > m_order.size()) {
        qCritical() << "Error in GroupOrder::add: depth" << depth << " does not exist! There are only " << m_order.size() << " depths";
        return;
    }
    remove(groupId);
    if (depth == m_order.size()) {
        m_order.push_back({groupId});
        return;
    }
    m_order[depth].push_back(groupId);
}

/**
 * Remove the group id from the order list
 */
void GroupOrder::remove(int groupId) {
    for (std::vector<int> &depth : m_order) {
        depth.erase(std::remove_if(depth.begin(), depth.end(), [&groupId](int el) {
            return el == groupId;
        }), depth.end());
    }
    // Delete empty depths
    m_order.erase(std::remove_if(m_order.begin(), m_order.end(), [](const std::vector<int> &el) {
        return el.empty();
    }), m_order.end());
    if (m_order.empty()) {
        m_order.emplace_back();
    }
}   

/**
 * Put group A at a new depth just above group B.
 * Return the index of the newly added depth
 */
int GroupOrder::setAOnTopOfB(int groupIdA, int groupIdB) {
    remove(groupIdA);
    int depthB = depthOf(groupIdB);
    m_order.insert(std::next(m_order.begin(), depthB), {groupIdA});
    return depthB;
}

/**
 * Put group A at a new depth just under group B.
 * Return the index of the newly added depth
 */
int GroupOrder::setAUnderB(int groupIdA, int groupIdB) {
    remove(groupIdA);
    int depthB = depthOf(groupIdB);
    auto res = m_order.insert(std::next(m_order.begin(), depthB + 1), {groupIdA});
    return depthB + 1;
}

/**
 * Put group A and B at the same depth.
 * More specifically group B is put at the depth of group A. 
 */
void GroupOrder::sameDepth(int groupIdA, int groupIdB) {
    remove(groupIdB);
    add(groupIdB, depthOf(groupIdA));
}

/**
 * Set all group to the same depth
 */
void GroupOrder::reset() {
    std::vector<int> newOrder;
    for (int i = 0; i < m_order.size(); ++i) {
        for (int j = 0; j < m_order[i].size(); ++j) {
            newOrder.push_back(m_order[i][j]);
        }
    }
    m_order.clear();
    m_order.push_back(newOrder);
}

void GroupOrder::setParentKeyFrame(VectorKeyFrame *keyframe) {
    m_parentKeyFrame = keyframe;
}

/**
 * Return the depth of the given group id
 * Return -1 if the given group id is not in the list
 */
int GroupOrder::depthOf(int groupId) {
    for (int i = 0; i < m_order.size(); ++i) {
        for (int j = 0; j < m_order[i].size(); ++j) {
            if (m_order[i][j] == groupId) {
                return i;
            }
        }
    }
    return -1;
}

/**
 * Return true if the other group order is the same as this instance 
 */
bool GroupOrder::sameOrder(const GroupOrder &other) const {
    if (other.m_order.size() != m_order.size()) return false;
    for (int i = 0; i < m_order.size(); ++i) {
        if (other.m_order[i].size() != m_order[i].size() || !std::is_permutation(other.m_order[i].begin(), other.m_order[i].end(), m_order[i].begin())) return false;
    }
    return true;
}

void GroupOrder::load(const QDomElement &el) {
    m_order.clear();
    int depths = el.attribute("size").toInt();
    m_order.resize(depths);
    int d = 0;
    QDomNode node = el.firstChild();
    while (!node.isNull()) {
        QDomElement depth  = node.toElement();
        QString startPos = depth.text();
        QTextStream streamStart(&startPos);
        int nbGroups = depth.attribute("size").toInt();
        int id;
        for (int i = 0; i < nbGroups; ++i) {
            streamStart >> id;
            add(id, d);
        }
        d++;
        node = node.nextSibling();
    }
}

void GroupOrder::save(QDomElement &el) const {
    el.setAttribute("size", (int)nbDepths());
    for (int i = 0; i < nbDepths(); ++i) {
        QDomElement depth = el.ownerDocument().createElement("depth");
        depth.setAttribute("size", (int)m_order[i].size());
        QString groupsId;
        QTextStream startPos(&groupsId);
        for (int id : m_order[i]) {
            startPos << id << " ";
        }
        QDomText txt = el.ownerDocument().createTextNode(groupsId);
        depth.appendChild(txt);
        el.appendChild(depth);
    }
}

void GroupOrder::debug() const {
    for (int i = 0; i < m_order.size(); ++i) {
        std::cout << "Depth: " << i << std::endl;
        for (int j = 0; j < m_order[i].size(); ++j) {
            std::cout << m_order[i][j] << " ";
        }
        std::cout << std::endl;
    }
}