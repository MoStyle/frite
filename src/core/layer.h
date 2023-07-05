/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LAYER_H
#define LAYER_H

#include <QtWidgets>
#include <QDomElement>

#include "point.h"

class KeyFrame;
class TimeLineCells;
class VectorKeyFrame;
class Editor;
class KeyframedVector;
class KeyframedFloat;

typedef QMap<int, VectorKeyFrame*>::const_iterator keyframe_iterator;
typedef QPair<int, int> KeyframeGroup; // keyframeId, groupId

class Layer : public QObject {
    Q_OBJECT
   public:
    Layer(QObject* parent, Editor* editor);
    virtual ~Layer();

    bool load(QDomElement& element, const QString& path);
    bool save(QDomDocument& doc, QDomElement& root, const QString& path) const;

    int id() const { return m_id; }

    void paintTrack(QPainter& painter, TimeLineCells* cells, int x, int y, int height, bool selected);
    void paintKeys(QPainter& painter, TimeLineCells* cells, int y, bool selected);
    void paintLabel(QPainter& painter, int x, int y, int height, int width, bool selected);
    void paintSelection(QPainter& painter, int x, int y, int height, int width);

    QString name() { return m_name; }
    void setName(const QString& name) { m_name = name; }

    void deselectAllKeys();
    void switchVisibility() { m_visible = !m_visible; }
    bool visible() { return m_visible; }

    void switchShowOnion() { m_showOnion = !m_showOnion; }
    bool showOnion() { return m_showOnion; }

    qreal opacity() const { return m_opacity; }
    void setOpacity(qreal opacity) { m_opacity = opacity; }

    int getSelectedFrame() { return m_selectedFrame; }

    int getSizeKey(int frame) { return getNextKeyFramePosition(frame) - getPreviousKeyFramePosition(frame); }

    void startMoveKeyframe(TimeLineCells* cells, QMouseEvent* event, int frameNumber, int y);
    void moveKeyframe(QMouseEvent* event, int frameNumber);
    void stopMoveKeyframe(QMouseEvent* event, int layerNumber, int frameNumber);

    VectorKeyFrame* addNewEmptyKeyAt(int frame);

    bool keyExists(int frame);
    int nbKeys() const { return m_keyFrames.size(); }
    int firstKeyFramePosition();
    int getMaxKeyFramePosition();
    int getPreviousKeyFramePosition(int frame);
    int getLastKeyFramePosition(int frame);
    int getNextKeyFramePosition(int frame);
    int getVectorKeyFramePosition(VectorKeyFrame* keyframe);
    KeyFrame* getKeyFrameAt(int frame);
    KeyFrame* getLastKeyFrameAtPosition(int frame);
    VectorKeyFrame* getLastVectorKeyFrameAtFrame(int frame, int increment);
    VectorKeyFrame* getVectorKeyFrameAtFrame(int frame);
    VectorKeyFrame* getNextKey(int frame);
    VectorKeyFrame* getLastKey(int frame);
    VectorKeyFrame* getPrevKey(int frame);
    VectorKeyFrame* getNextKey(VectorKeyFrame *keyframe);
    VectorKeyFrame* getPrevKey(VectorKeyFrame *keyframe);
    QMap<int, VectorKeyFrame*>::const_iterator keysBegin() const { return m_keyFrames.constBegin(); }
    QMap<int, VectorKeyFrame*>::const_iterator keysEnd() const { return m_keyFrames.constEnd(); }
    int stride(int frame);
    int inbetweenPosition(int frame);

    int getPreviousFrameNumber(int position, bool keyMode);
    int getNextFrameNumber(int position, bool keyMode);

    void insertKeyFrame(int frame, VectorKeyFrame* keyframe);
    void removeKeyFrame(int frame);
    void moveKeyFrame(int oldFrame, int newFrame);

    QList<int> keys() const { return m_keyFrames.keys(); }
    Editor *editor() const { return m_editor; }

    QColor color = Qt::black;

   private:
    QMap<int, VectorKeyFrame*> m_keyFrames;
    QMap<int, VectorKeyFrame*> m_backup;

    QRect topRect(TimeLineCells* cells, int frameNumber, int y);
    QRect bottomRect(TimeLineCells* cells, int frameNumber, int y, int length);

    static int m_staticIdx;
    int m_id;
    QString m_name;
    bool m_visible;
    bool m_showOnion;
    qreal m_opacity;

    int m_frameClicked;
    int m_selectedFrame;
    int m_backupSelectedFrame;
    int m_backupClickedFrame;

    Editor* m_editor;

    const int m_squareSize = 6;
};

#endif  // LAYER_H
