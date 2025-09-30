/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "groupswidget.h"
#include "editor.h"
#include "playbackmanager.h"
#include "layermanager.h"
#include "grouplistwidget.h"
#include "groupinfowidget.h"

#include <QtWidgets>
#include <QSplitter>

GroupsWidget::GroupsWidget(Editor *editor, QWidget *parent) : QDockWidget(parent), m_editor(editor) {
    setObjectName(tr("Groups"));
    setWindowTitle(tr("Groups"));
    setFloating(true);
    setFocusPolicy(Qt::NoFocus);

    m_content = new QWidget(this);
    m_layout = new QVBoxLayout();
    m_layout->setContentsMargins(0, 0, 0, 0);

    // m_hScrollBar = new QScrollBar(Qt::Horizontal);
    // m_vScrollBar = new QScrollBar(Qt::Vertical);
    // m_vScrollBar->setMinimum(0);
    // m_vScrollBar->setMaximum(1);
    // m_vScrollBar->setPageStep(1);
    // m_layout->addWidget(m_vScrollBar);
    // m_layout->addWidget(m_hScrollBar);

    // m_layout->addWidget(new GroupInfoWidget(m_editor, nullptr, this));


    m_postGroupsWidget = new GroupListWidget(m_editor, this);
    m_preGroupsWidget = new GroupListWidget(m_editor, this);

    QSplitter *splitter = new QSplitter(m_content);
    splitter->addWidget(m_postGroupsWidget);
    splitter->addWidget(m_preGroupsWidget);
    splitter->setOrientation(Qt::Vertical);
    // splitter->setSizes(QList<int>() << 400 << 200);

    m_layout->insertWidget(0, splitter, Qt::AlignTop | Qt::AlignLeft);
    // m_layout->insertWidget(1, m_postGroupsWidget, Qt::AlignTop | Qt::AlignLeft);
    // m_layout->insertWidget(2, m_preGroupsWidget, Qt::AlignTop | Qt::AlignLeft);
    m_layout->setSpacing(0);


    setWidget(m_content);
    m_content->setLayout(m_layout);
}

GroupsWidget::~GroupsWidget() {
    m_postGroupsWidget->clearAll();
    m_preGroupsWidget->clearAll();
    delete m_postGroupsWidget;
    delete m_preGroupsWidget;
}

void GroupsWidget::keyframeChanged(int frame) {
    refresh(m_editor->layers()->currentLayerIndex(), frame);
}

void GroupsWidget::layerChanged(int layer) {
    refresh(layer, m_editor->playback()->currentFrame());
}

void GroupsWidget::updateGroup(GroupType type, int id) {
    QWidget *groupWidget;
    switch (type) {
        case PRE:
            groupWidget = m_preGroupsWidget->groupInfoWidget(id);
            break;
        case POST:
            groupWidget = m_postGroupsWidget->groupInfoWidget(id);
            break;
        default:
            qWarning() << "Invalid group type in updateGroup: " << type;
            return;
    }

    if (groupWidget == nullptr) {
        qWarning() << "Invalid group of type " << type << " and id " << id << " in updateGroup";
        return;
    }

    groupWidget->update();
}

void GroupsWidget::updateGroups(GroupType type) {
    switch (type) {
        case PRE:
            m_preGroupsWidget->update();
            break;
        case POST:
            m_postGroupsWidget->update();
            break;
        default:
            qWarning() << "Invalid group type in updateGroups: " << type;
            return;
    }
}

void GroupsWidget::refreshGroups(GroupType type) {
    Layer *layer = m_editor->layers()->currentLayer();
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(m_editor->playback()->currentFrame(), 0);
    if (keyframe == nullptr) return;
    switch (type) {
        case PRE:
            m_preGroupsWidget->clearAll();
            m_preGroupsWidget->updateAll(keyframe->preGroups());
            break;
        case POST:
            m_postGroupsWidget->clearAll();
            m_postGroupsWidget->updateAll(keyframe->postGroups());
            break;
        default:
            qWarning() << "Invalid group type in refreshGroups: " << type;
            return;
    }
}

void GroupsWidget::refresh(int layerIdx, int frame) {
    clearAll();

    Layer *layer = m_editor->layers()->layerAt(layerIdx);
    VectorKeyFrame *keyframe = layer->getLastVectorKeyFrameAtFrame(frame, 0);
    if (keyframe == nullptr) return;

    m_postGroupsWidget->clearAll();
    m_postGroupsWidget->updateAll(keyframe->postGroups());

    m_preGroupsWidget->clearAll();
    m_preGroupsWidget->updateAll(keyframe->preGroups());

    int nbCells = 4 + keyframe->postGroups().size() + keyframe->preGroups().size();
    setMaximumSize(width(), nbCells * 25);
    setMinimumSize(width(), nbCells * 25);
}

void GroupsWidget::clearAll() {
    m_postGroupsWidget->clearAll();
    m_preGroupsWidget->clearAll();
}