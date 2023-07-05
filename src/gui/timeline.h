/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
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
