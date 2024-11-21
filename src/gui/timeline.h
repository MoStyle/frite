/*

Pencil - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation;

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef TIMELINE_H
#define TIMELINE_H

#include <QDockWidget>
#include <QScrollBar>

class TimeLineCells;
class TimeControls;
class Editor;
class Layer;

class TimeLine : public QDockWidget
{
    Q_OBJECT

signals :
    void lengthChange(QString);
    void frameSizeChange(int);
    void fontSizeChange(int);
    void labelChange(int);
    void scrubChange(int);
    void deleteCurrentLayer();
    void currentLayerChanged(int);
    void currentFrameChanged(int);
    void newLayer();

public slots:
    void updateFrame(int frameNumber);
    void updateLayerView();
    void updateLength();
    void updateContent();

public:
    TimeLine(Editor* editor, QWidget *parent = nullptr);

    bool scrubbing;
    int  getFrameLength();

    TimeControls* timeControls() { return m_timeControls; }

protected:
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent*) override;

private:
    Editor*        m_editor;
    QScrollBar*    m_hScrollBar;
    QScrollBar*    m_vScrollBar;
    TimeLineCells* m_tracks;
    TimeLineCells* m_layersNames;
    TimeControls*  m_timeControls;

    int m_lastUpdatedFrame = 0;
};

#endif
