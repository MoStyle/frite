/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __GROUPINFOWIDGET_H__
#define __GROUPINFOWIDGET_H__

#include <QWidget>

class Editor;
class Group;

class GroupInfoWidget : public QWidget {
    Q_OBJECT

public:
    GroupInfoWidget(Editor *editor, Group *group, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    // void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void drawCellBackground(QPainter &painter, int x, int y, int width, int height);

    Editor *m_editor;
    Group *m_group;
    QString m_name;
    static const int CELL_HEIGHT = 25;
};

#endif // __GROUPINFOWIDGET_H__