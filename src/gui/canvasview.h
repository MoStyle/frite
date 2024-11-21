#ifndef CANVAS_VIEW_H
#define CANVAS_VIEW_H

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QWheelEvent>

class Editor;

class CanvasView : public QGraphicsView
{
    Q_OBJECT

public:
    CanvasView(QGraphicsScene *scene, Editor *editor, QWidget *parent = nullptr, bool mouseEventsTransparent=true);
    CanvasView(Editor *editor, QWidget *parent = nullptr);
    ~CanvasView();

protected:
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void wheelEvent(QWheelEvent *event) Q_DECL_OVERRIDE;
    void tabletEvent(QTabletEvent *event) Q_DECL_OVERRIDE;
    bool event(QEvent *event) Q_DECL_OVERRIDE;
    
private:
    void init(bool mouseEventTransparent=true);
    Editor *m_editor;
    int lastFrame;
};

#endif