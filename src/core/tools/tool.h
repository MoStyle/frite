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
    enum ToolType { 
        Pen, 
        DrawEndKeyframe, 
        Eraser, 
        Hand, 
        Select, 
        RigidDeform, 
        Warp, 
        StrokeDeform, 
        RegistrationLasso, 
        MaskPen, 
        Scribble, 
        Traj, 
        DrawTraj, 
        TrajTangent, 
        Lasso, 
        Correspondence, 
        FillGrid, 
        DirectMatching, 
        PivotCreation, 
        PivotEdit, 
        PivotTangent, 
        PivotRotation, 
        PivotScaling, 
        PivotTranslation, 
        MoveFrames, 
        Halves, 
        SimplifySpacing, 
        ProxySpacing, 
        MovePartials, 
        GroupOrdering, 
        LocalMask, 
        CopyStrokes, 
        Visibility, 
        Debug, 
        NoTool 
    };
    
    Q_ENUM(ToolType)
    struct EventInfo {
        VectorKeyFrame *key;
        QPointF firstPos;
        QPointF lastPos;
        QPointF pos;
        float rotation;
        float pressure = 1.0f;
        double alpha;
        int inbetween;
        int stride;
        Qt::KeyboardModifiers modifiers;
        Qt::MouseButton mouseButton;
    };

    struct WheelEventInfo {
        VectorKeyFrame *key;
        double alpha;
        double delta; // in pixels or wheel rotation angle (fallback)
        QPointF pos;
        Qt::KeyboardModifiers modifiers;
    };

    explicit Tool(QObject *parent, Editor *editor) : QObject(parent), m_editor(editor), m_chartTool(false), m_contextMenuAllowed(true), m_needEscapeFocus(false), m_needReturnFocus(false) { }
    
    virtual ~Tool() { }

    virtual ToolType toolType() const = 0;

    virtual QCursor makeCursor(float scaling=1.0f) const = 0;

    virtual bool isChartTool() const { return m_chartTool; }

    virtual bool contextMenuAllowed() const { return m_contextMenuAllowed; }

    virtual bool needEscapeFocus() const { return m_needEscapeFocus; }

    virtual bool needReturnFocus() const { return m_needReturnFocus; }

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
    virtual void drawUI(QPainter &painter, VectorKeyFrame *key) {}
    virtual void drawGL(VectorKeyFrame *key, qreal alpha) {}
    virtual void contextMenu(QMenu &contextMenu) {}
    virtual void frameChanged(int frame) { }

protected:
    Editor *m_editor;
    QString m_toolTips;
    bool m_chartTool;
    bool m_contextMenuAllowed;
    bool m_needEscapeFocus;
    bool m_needReturnFocus;
};

#endif