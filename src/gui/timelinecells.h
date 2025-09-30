/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TIMELINECELLS_H
#define TIMELINECELLS_H

#include <QtWidgets>

class TimeLine;
class Editor;
class Layer;

typedef enum {
    TYPE_TRACKS,
    TYPE_LAYER_ATTR,
    TYPE_UNDEFINED
} TimeLineCellsTypes;

class TimeLineCells : public QWidget
{
    Q_OBJECT

public:

    TimeLineCells(TimeLine* parent = 0, Editor* editor = 0, TimeLineCellsTypes type = TYPE_UNDEFINED);
    ~TimeLineCells();
    int getLayerNumber(int y);
    int getLayerY(int layerNumber);
    int getFrameNumber(int x);
    int getFrameX(int frameNumber);
    int getMouseMoveY() { return mouseMoveY; }
    int getOffsetY() { return offsetY; }
    int getLayerHeight() { return layerHeight; }
    int getFrameLength() {return frameLength;}
    int getFrameSize() { return frameSize; }
    bool selectionContainsVectorKeyFrame(int frame);

signals:
    void mouseMovedY(int);
    void currentLayerChanged(int);
    void currentFrameChanged(int);
    void lengthChanged(int);

public slots:
    void updateContent();
    void updateFrame(int frameNumber);
    void frameSizeChange(int);
    void fontSizeChange(int);
    void labelChange(int);
    void hScrollChange(int);
    void vScrollChange(int);
    void setMouseMoveY(int x) { mouseMoveY = x;}

protected:
    void drawContent();
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void changeExposure();
    void deleteImage();

    void automaticRegistration();
    void pasteKeyFrame();
    void pasteKeyFrameAtTheEnd();
    void pasteMultipleKeyFrame();
    void pasteMultipleKeyFrameAtTheEnd();

private:
    TimeLine* timeLine;
    Editor* m_editor;
    TimeLineCellsTypes type;
    QPixmap* cache;
    bool drawFrameNumber;
    int frameLength;
    int frameSize;
    int fontSize;
    bool scrubbing;
    int layerHeight;
    int offsetX, offsetY;
    int startY, endY, m_startLayerNumber, m_frameNumber ;
    int mouseMoveY;
    int frameOffset, layerOffset;
    bool m_isChangingOpacity;
    qreal m_prevOpacity;

    QPoint m_selectionBoxOrigin;
    QRubberBand m_selectionBox;
};

#endif // TIMELINECELLS_H
