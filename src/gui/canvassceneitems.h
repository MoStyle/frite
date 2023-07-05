/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __CANVASSCENEITEMS_H__
#define __CANVASSCENEITEMS_H__

#include <QGraphicsItem>

class Group;
class PickTool;

class GroupSelectionOutline : public QGraphicsItem {
public:
    GroupSelectionOutline(const QHash<int, Group *> &groups);
    
protected:
    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

private:
    QHash<int, Group *> m_groups;
    QRectF m_boundingRect;
};

class LassoDrawer : public QGraphicsItem {
public:
    LassoDrawer(PickTool *tool) : QGraphicsItem(nullptr), m_tool(tool) { }

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

private:
    PickTool *m_tool;
};

#endif // __CANVASSCENEITEMS_H__