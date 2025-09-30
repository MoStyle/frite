/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __GROUPSWIDGET_H__
#define __GROUPSWIDGET_H__

#include <QDockWidget>
#include <QScrollBar>
#include <QVBoxLayout>

#include "group.h"

class Editor;
class VectorKeyFrame;
class GroupListWidget;

class GroupsWidget : public QDockWidget {
    Q_OBJECT

public:
    GroupsWidget(Editor *editor, QWidget *parent = nullptr);
    ~GroupsWidget();

    void updateContent();

public slots:
    void keyframeChanged(int frame);
    void layerChanged(int layer);
    void updateGroup(GroupType type, int id);
    void updateGroups(GroupType type);
    void refreshGroups(GroupType type);

protected:
    // void paintEvent(QPaintEvent* event) override;
    // void resizeEvent(QResizeEvent* event) override;
    // void mousePressEvent(QMouseEvent* event) override;
    // void mouseMoveEvent(QMouseEvent* event) override;
    // void mouseReleaseEvent(QMouseEvent* event) override;
    // void mouseDoubleClickEvent(QMouseEvent* event) override;
    // void contextMenuEvent(QContextMenuEvent *event) override;

private:
    // void drawContent();
    // void drawGroupContent(const GroupList &groupList);
    // void drawCellBackground(QPainter &painter, int x, int y, int width, int height);
    void refresh(int layerIdx, int frame);
    void clearAll();

    Editor *m_editor;

    QWidget *m_content;
    QVBoxLayout *m_layout;
    QScrollBar *m_hScrollBar;
    QScrollBar *m_vScrollBar;

    GroupListWidget *m_postGroupsWidget;
    GroupListWidget *m_preGroupsWidget;
};

#endif // __GROUPSWIDGET_H__