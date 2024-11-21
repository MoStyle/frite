#include "grouplistwidget.h"
#include "grouplist.h"
#include "groupinfowidget.h"
#include "editor.h"
#include "playbackmanager.h"

#include <QVBoxLayout>

GroupListWidget::GroupListWidget(Editor *editor, QWidget *parent) : QWidget(parent), m_editor(editor) {
    m_layout = new QVBoxLayout();
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    m_headerLabel = new QLabel("", this);
    m_layout->insertWidget(0, m_headerLabel);
    setLayout(m_layout);
}

void GroupListWidget::updateAll(const GroupList& groupList) {
    clearAll();
    int idx = 1;

    VectorKeyFrame *keyframe = groupList.parentKeyframe();
    double alpha = m_editor->alpha(m_editor->playback()->currentFrame(), keyframe->parentLayer());

    if (groupList.type() == POST) {
        m_headerLabel->setText("Post groups");

        for (int i = 0; i < keyframe->orderPartials().lastPartialAt(alpha).groupOrder().order().size(); ++i) {
            const auto &depth = keyframe->orderPartials().lastPartialAt(alpha).groupOrder().order().at(i);
            for (int groupId : depth) {
                Group *group = groupList.fromId(groupId);
                QWidget *groupInfo = new GroupInfoWidget(m_editor, group, this);
                m_groupWidgets.insert(group->id(), groupInfo);
                m_layout->insertWidget(idx, groupInfo, Qt::AlignTop | Qt::AlignLeft);
                idx++;
            }
        }
    } else {
        m_headerLabel->setText("Pre groups");
        for (Group *group : groupList) {
            QWidget *groupInfo = new GroupInfoWidget(m_editor, group, this);
            m_groupWidgets.insert(group->id(), groupInfo);
            m_layout->insertWidget(idx, groupInfo, Qt::AlignTop | Qt::AlignLeft);
            idx++;
        }
    }
}

// void GroupListWidget::refreshAll() {
//     for (QWidget *widget : m_groupWidgets) {
//         if (widget == nullptr) {
//             qWarning() << "Invalid widget in refreshAll";
//             continue;
//         }
//         widget->update();
//     }
// }

void GroupListWidget::clearAll() {
    for (QWidget *widget : m_groupWidgets) {
        if (widget == nullptr) continue;
        m_layout->removeWidget(widget);
        delete widget;
    }
    m_groupWidgets.clear();
    m_groupWidgets.squeeze();
}

QWidget *GroupListWidget::groupInfoWidget(int id) const {
    if (m_groupWidgets.contains(id)) {
        return m_groupWidgets.value(id);
    }
    return nullptr;
}
