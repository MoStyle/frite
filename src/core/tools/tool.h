/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef TOOL_H
#define TOOL_H

class VectorKeyFrame;
class Editor;

#include <QObject>
#include <QGraphicsItem>
#include <QPainter>
#include <QTransform>
#include <QKeyEvent>

#include "point.h"

class Tool : public QObject {
    Q_OBJECT

public:
    enum ToolType { Pen, DrawEndKeyframe, Eraser, Hand, Select, RigidDeform, Warp, StrokeDeform, RegistrationLasso, Scribble, Traj, DrawTraj, TrajTangent, Lasso, Correspondence, FillGrid, DirectMatching, MoveFrames, Halves, SimplifySpacing, NoTool };
    Q_ENUM(ToolType)
    struct EventInfo {
        VectorKeyFrame *key;
        QPointF firstPos;
        QPointF lastPos;
        QPointF pos;
        float rotation;
        float pressure = 1.0f;
        float alpha;
        int inbetween;
        int stride;
        Qt::KeyboardModifiers modifiers;
        Qt::MouseButton mouseButton;
    };

    struct WheelEventInfo {
        VectorKeyFrame *key;
        float alpha;
        double delta; // in pixels or wheel rotation angle (fallback)
        QPointF pos;
        Qt::KeyboardModifiers modifiers;
    };

    explicit Tool(QObject *parent, Editor *editor) : QObject(parent), m_editor(editor), m_spacingTool(false), m_contextMenuAllowed(true) { }
    virtual ~Tool() { }

    virtual ToolType toolType() const = 0;

    virtual QGraphicsItem *graphicsItem() = 0;

    virtual QCursor makeCursor(float scaling=1.0f) const = 0;

    virtual bool isSpacingTool() const { return m_spacingTool; }

    virtual bool contextMenuAllowed() const { return m_contextMenuAllowed; }

signals:
    void updateFrame();

public slots:
    virtual void toggled(bool on);
    virtual void pressed(const EventInfo& info) {}
    virtual void moved(const EventInfo& info) {}
    virtual void released(const EventInfo& info) {}
    virtual void doublepressed(const EventInfo& info) {}
    virtual void wheel(const WheelEventInfo& info) {}
    virtual void keyPressed(QKeyEvent *event) {}
    virtual void keyReleased(QKeyEvent *event) {}
    virtual void draw(QPainter &painter, VectorKeyFrame *key) {}
    virtual void contextMenu(QMenu &contextMenu) {}

protected:
    Editor *m_editor;
    QString m_toolTips;
    bool m_spacingTool;
    bool m_contextMenuAllowed;
};

#endif