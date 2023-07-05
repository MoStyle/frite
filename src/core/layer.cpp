/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtWidgets>

#include "editor.h"
#include "keycommands.h"
#include "layer.h"
#include "tabletcanvas.h"
#include "timelinecells.h"
#include "vectorkeyframe.h"
#include "gridmanager.h"

int Layer::m_staticIdx = 0;

class GridManager;

Layer::Layer(QObject *parent, Editor *editor)
    : QObject(parent),
      m_name("Layer"),
      m_visible(true),
      m_showOnion(false),
      m_opacity(1.0),
      m_frameClicked(-1),
      m_selectedFrame(-1),
      m_backupSelectedFrame(-1),
      m_editor(editor) {
    m_id = ++m_staticIdx;
    m_keyFrames[1] = new VectorKeyFrame(this);  // virtual invisible key at the end of the map
}

Layer::~Layer() {
    qDeleteAll(m_keyFrames.values());
    m_keyFrames.clear();
    if (m_id == m_staticIdx) m_staticIdx--;
}

bool Layer::load(QDomElement &element, const QString &path) {
    if (element.tagName() != "layer") return false;

    if (!element.attribute("id").isNull()) {
        int id = element.attribute("id").toInt();
        m_id = id;
    }
    m_name = element.attribute("name", "Layer");
    m_visible = (element.attribute("visibility", "1").toInt() == 1);
    m_opacity = element.attribute("opacity", "1.0").toDouble();

    qDeleteAll(m_keyFrames.values());
    m_keyFrames.clear();

    // load keyframes
    QDomNode keyTag = element.firstChild();
    while (!keyTag.isNull()) {
        QDomElement keyElement = keyTag.toElement();
        VectorKeyFrame *prevKey = nullptr;
        if (!keyElement.isNull()) {
            if (keyElement.tagName() == "vectorkeyframe") {
                int frame = keyElement.attribute("frame").toInt();
                VectorKeyFrame *keyFrame = new VectorKeyFrame(this);
                keyFrame->load(keyElement, path, m_editor);                
                if (m_keyFrames.contains(frame)) {
                    delete m_keyFrames[frame];
                }
                m_keyFrames[frame] = keyFrame;
            }
        }
        keyTag = keyTag.nextSibling();
    }

    // set next/prev trajectories pointers
    for (VectorKeyFrame *key : m_keyFrames) {
        VectorKeyFrame *next = key->nextKeyframe();
        VectorKeyFrame *prev = key->prevKeyframe();
        for (const std::shared_ptr<Trajectory> &traj : key->trajectories()) {
            if (traj->nextTrajectoryID() >= 0)
                traj->setNextTrajectory(next->trajectories().value(traj->nextTrajectoryID(), nullptr));
            if (traj->prevTrajectoryID() >= 0)
                traj->setPrevTrajectory(prev->trajectories().value(traj->prevTrajectoryID(), nullptr));
        }
    }

    // retrocomp lattices
    for (VectorKeyFrame *keyframe : m_keyFrames) {
        for (Group *group : keyframe->postGroups()) {
            if (group->lattice() != nullptr && group->lattice()->origin() == Eigen::Vector2i::Zero()) {
                group->lattice()->restoreKeysRetrocomp(group, m_editor);
                group->lattice()->isConnected();
            }
        }
    }

    qDebug() << "Loaded " << m_keyFrames.size() << " keyframes";

    return true;
}

bool Layer::save(QDomDocument &doc, QDomElement &root, const QString &path) const {
    QDomElement layerElt = doc.createElement("layer");
    layerElt.setAttribute("id", m_id);
    layerElt.setAttribute("name", m_name);
    layerElt.setAttribute("visibility", m_visible);
    layerElt.setAttribute("opacity", m_opacity);

    for (keyframe_iterator it = m_keyFrames.begin(); it != m_keyFrames.end(); ++it) {
        it.value()->save(doc, layerElt, path, m_id, it.key());
    }

    root.appendChild(layerElt);
    return true;
}

void Layer::paintLabel(QPainter &painter, int x, int y, int width, int height, bool selected) {
    painter.setBrush(QGuiApplication::palette().color(QPalette::Light));
    painter.setPen(QPen(QBrush(QGuiApplication::palette().color(QPalette::Dark)), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawRect(x, y - 1, width, height);  // empty rectangle  by default

    // visibility
    if (m_visible)
        painter.setBrush(QGuiApplication::palette().color(QPalette::Midlight));
    else
        painter.setBrush(Qt::NoBrush);
    painter.setPen(QGuiApplication::palette().color(QPalette::WindowText));
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawEllipse(x + 6, y + 4, 9, 9);

    // show onion skin
    if (m_showOnion)
        painter.setBrush(QGuiApplication::palette().color(QPalette::Midlight));
    else
        painter.setBrush(Qt::NoBrush);
    painter.setPen(QGuiApplication::palette().color(QPalette::WindowText));
    painter.drawEllipse(x + 23, y + 4, 9, 9);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // opacity
    painter.setBrush(QGuiApplication::palette().color(QPalette::Midlight));
    painter.drawRect(150, y + 2, 35, height - 6);
    painter.setBrush(QGuiApplication::palette().color(QPalette::Light));
    painter.drawRect(150 + int(m_opacity * 30), y + 1, 5, height - 4);

    if (selected) {
        paintSelection(painter, x, y, width, height);
    }

    QFont f = QApplication::font();
    f.setPointSize(height / 2);
    painter.setFont(f);
    painter.setPen(QGuiApplication::palette().color(QPalette::ButtonText));
    painter.drawText(QPoint(x + 40, y + (2 * height) / 3), m_name);
}

void Layer::paintSelection(QPainter &painter, int x, int y, int width, int height) {
    QLinearGradient linearGrad(QPointF(0, y), QPointF(0, y + height));

    QColor base = QGuiApplication::palette().color(QPalette::Button);
    base.setAlpha(100);
    linearGrad.setColorAt(0, base);
    base.setAlpha(80);
    linearGrad.setColorAt(0.10, base);
    base.setAlpha(64);
    linearGrad.setColorAt(0.20, base);
    base.setAlpha(20);
    linearGrad.setColorAt(0.35, base);
    linearGrad.setColorAt(0.351, QColor(0, 0, 0, 32));
    linearGrad.setColorAt(0.66, QColor(245, 245, 245, 32));
    linearGrad.setColorAt(1, QColor(235, 235, 235, 128));

    painter.setBrush(linearGrad);
    painter.setPen(Qt::NoPen);
    painter.drawRect(x, y, width, height - 1);
}

void Layer::paintTrack(QPainter &painter, TimeLineCells *cells, int x, int y, int width, bool selected) {
    int height = cells->getLayerHeight();
    QFont f = QApplication::font();
    f.setPointSize(height / 2);
    painter.setFont(f);
    if (m_visible) {
        QColor col = QGuiApplication::palette().color(QPalette::Light);

        painter.setBrush(col);
        painter.setPen(QPen(QBrush(QGuiApplication::palette().color(QPalette::Dark)), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawRect(x, y - 1, width, height);

        paintKeys(painter, cells, y, selected);

        if (selected) paintSelection(painter, x, y, width, height);
    } else {
        painter.setBrush(QGuiApplication::palette().color(QPalette::Inactive, QPalette::Midlight));
        painter.setPen(QPen(QBrush(QGuiApplication::palette().color(QPalette::Dark)), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawRect(x, y - 1, width, height);  // empty rectangle  by default
    }
}

QRect Layer::topRect(TimeLineCells *cells, int frameNumber, int y) {
    return QRect(cells->getFrameX(frameNumber) - cells->getFrameSize(), y + 1, m_squareSize, m_squareSize);
}

QRect Layer::bottomRect(TimeLineCells *cells, int frameNumber, int y, int length) {
    return QRect(cells->getFrameX(frameNumber) + length * cells->getFrameSize() - m_squareSize,
                 y + cells->getLayerHeight() - 3 - m_squareSize, m_squareSize, m_squareSize);
}

void Layer::paintKeys(QPainter &painter, TimeLineCells *cells, int y, bool selected) {
    if (m_visible) {
        QList<int> keyFrameIndices = keys();
        for (int i = 0; i < keyFrameIndices.size() - 1; i++) {  // the last keyframe is invisible
            painter.setPen(QPen(QBrush(QColor(40, 40, 40)), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            if (!selected)
                painter.setBrush(Qt::NoBrush);
            else
                painter.setBrush(QGuiApplication::palette().color(QPalette::Midlight));

            int currentFrame = keyFrameIndices[i];
            int length = 1;
            if (i + 1 < keyFrameIndices.size()) length = keyFrameIndices[i + 1] - currentFrame;

            painter.drawRect(cells->getFrameX(currentFrame) - cells->getFrameSize(), y + 1, length * cells->getFrameSize(),
                             cells->getLayerHeight() - 4);

            if (m_keyFrames[keyFrameIndices[i]]->isTopSelected()) painter.setBrush(QGuiApplication::palette().color(QPalette::Dark));
            painter.drawRect(topRect(cells, currentFrame, y));

            if (m_keyFrames[keyFrameIndices[i]]->isBottomSelected())
                painter.setBrush(QGuiApplication::palette().color(QPalette::Dark));
            else if (selected)
                painter.setBrush(QGuiApplication::palette().color(QPalette::Midlight));
            painter.drawRect(bottomRect(cells, currentFrame, y, length - 1));

            painter.setPen(QGuiApplication::palette().color(QPalette::Text));
            painter.setBrush(QGuiApplication::palette().color(QPalette::Text));
            QFont f = QApplication::font();
            f.setPixelSize(cells->getLayerHeight() / 3.0);
            painter.setFont(f);
            painter.drawText(QPointF(cells->getFrameX(currentFrame) + (length - 1) * cells->getFrameSize() - 8, y + 9),
                             QString::number(length));
        }
    }
}

void Layer::deselectAllKeys() {
    keyframe_iterator i = m_keyFrames.begin();
    while (i != m_keyFrames.end()) {
        i.value()->setTopSelected(false);
        i.value()->setBottomSelected(false);
        ++i;
    }
}

void Layer::startMoveKeyframe(TimeLineCells *cells, QMouseEvent *event, int frameNumber, int y) {
    keyframe_iterator it = m_keyFrames.upperBound(frameNumber);
    if (it == m_keyFrames.constEnd()) {
        deselectAllKeys();
        return;
    }

    if (it != m_keyFrames.constBegin()) it--;

    m_frameClicked = frameNumber;
    m_selectedFrame = it.key();

    QRect top = topRect(cells, m_selectedFrame, y);
    if (top.contains(event->pos())) {
        m_keyFrames[m_selectedFrame]->setTopSelected(true);
        m_keyFrames[m_selectedFrame]->setBottomSelected(false);
        m_backup = m_keyFrames;
        m_backupSelectedFrame = m_selectedFrame;
        m_backupClickedFrame = m_frameClicked;
        return;
    }

    ++it;
    int lenght = (it != m_keyFrames.constEnd()) ? (it.key() - m_selectedFrame) : 1;

    QRect bottom = bottomRect(cells, m_selectedFrame, y, lenght - 1);
    if (bottom.contains(event->pos())) {
        m_keyFrames[m_selectedFrame]->setBottomSelected(true);
        m_keyFrames[m_selectedFrame]->setTopSelected(false);
        m_backup = m_keyFrames;
        m_backupSelectedFrame = m_selectedFrame;
        m_backupClickedFrame = m_frameClicked;
        return;
    }
    m_keyFrames[m_selectedFrame]->setTopSelected(false);
    m_keyFrames[m_selectedFrame]->setBottomSelected(false);
    m_selectedFrame = -1;
}

void Layer::moveKeyframe(QMouseEvent *event, int frameNumber) {
    Q_UNUSED(event);
    if (m_selectedFrame > 0) {
        VectorKeyFrame *keyFrame = m_keyFrames[m_selectedFrame];
        if (keyFrame->isTopSelected() && frameNumber < getMaxKeyFramePosition() - 1) {
            int prev = m_selectedFrame == firstKeyFramePosition() ? 0 : getPreviousKeyFramePosition(m_selectedFrame);
            int next = getNextKeyFramePosition(m_selectedFrame);
            int moveTo = std::min(std::max(prev + 1, frameNumber), next - 1);
            if (moveTo != m_selectedFrame) {
                m_keyFrames.remove(m_selectedFrame);
                m_keyFrames[moveTo] = keyFrame;
                m_selectedFrame = moveTo;
                VectorKeyFrame *prev = getPrevKey(m_selectedFrame);
                if (prev) prev->updateCurves();                
            }
        } else if (keyFrame->isBottomSelected() && frameNumber >= m_selectedFrame) {
            // move following keyframes
            int offset = frameNumber - m_frameClicked;
            QList<int> keyFrameIndices = keys();
            if (offset > 0) {
                for (int i = keyFrameIndices.size() - 1; i >= 0; i--) {
                    int key = keyFrameIndices[i];
                    if (key > m_selectedFrame) {
                        moveKeyFrame(key, key + offset);
                    }
                }
            } else if (offset < 0) {
                for (int i = 0; i < keyFrameIndices.size(); i++) {
                    int key = keyFrameIndices[i];
                    if (key > m_selectedFrame) {
                        moveKeyFrame(key, key + offset);
                    }
                }
            }
            m_frameClicked = frameNumber;
        }
        m_editor->timelineUpdate(m_selectedFrame);
        keyFrame->updateCurves();
    }
    m_editor->tabletCanvas()->update();
}

void Layer::stopMoveKeyframe(QMouseEvent *event, int layerNumber, int frameNumber) {
    Q_UNUSED(event);

    m_selectedFrame = m_backupSelectedFrame;
    if (m_selectedFrame > 0) {
        m_keyFrames = m_backup;
        m_frameClicked = m_backupClickedFrame;
        VectorKeyFrame *keyFrame = m_keyFrames[m_selectedFrame];
        m_editor->undoStack()->beginMacro("Move keyframe");
        if (keyFrame->isTopSelected()) {
            int prev = m_selectedFrame == firstKeyFramePosition() ? 0 : getPreviousKeyFramePosition(m_selectedFrame);
            int next = getNextKeyFramePosition(m_selectedFrame);
            int moveTo = std::min(std::max(prev + 1, frameNumber), next - 1);
            m_editor->undoStack()->push(new MoveKeyCommand(m_editor, layerNumber, m_selectedFrame, moveTo));
        } else if (keyFrame->isBottomSelected()) {
            // move following keyframes
            int offset = std::max(frameNumber, m_selectedFrame) - m_frameClicked;
            QList<int> keyFrameIndices = keys();
            if (offset > 0) {
                for (int i = keyFrameIndices.size() - 1; i >= 0; i--) {
                    int key = keyFrameIndices[i];
                    if (key > m_selectedFrame) {
                        m_editor->undoStack()->push(new MoveKeyCommand(m_editor, layerNumber, key, key + offset));
                    }
                }
            } else if (offset < 0) {
                for (int i = 0; i < keyFrameIndices.size(); i++) {
                    int key = keyFrameIndices[i];
                    if (key > m_selectedFrame) {
                        m_editor->undoStack()->push(new MoveKeyCommand(m_editor, layerNumber, key, key + offset));
                    }
                }
            }
        }
        m_editor->undoStack()->endMacro();
        keyFrame->setTopSelected(false);
        keyFrame->setBottomSelected(false);
        m_selectedFrame = -1;
        m_backupSelectedFrame = -1;
    }
}

VectorKeyFrame *Layer::addNewEmptyKeyAt(int frame) {
    deselectAllKeys();
    VectorKeyFrame *keyframe = new VectorKeyFrame(this);
    int lastKey = m_keyFrames.lastKey();                    // invisible KF
    if (frame >= lastKey) moveKeyFrame(lastKey, frame + 1); // move the invisible KF after the new "last" KF
    insertKeyFrame(frame, keyframe);
    m_selectedFrame = frame;
    return keyframe;
}

bool Layer::keyExists(int frame) { return m_keyFrames.contains(frame); }

int Layer::firstKeyFramePosition() {
    if (!m_keyFrames.empty()) return m_keyFrames.firstKey();
    return 0;
}

int Layer::getMaxKeyFramePosition() {
    if (!m_keyFrames.empty()) return m_keyFrames.lastKey();
    return 0;
}

int Layer::getPreviousKeyFramePosition(int frame) {
    if (m_keyFrames.isEmpty()) return 0;

    keyframe_iterator it = m_keyFrames.lowerBound(frame);
    if (it == m_keyFrames.end()) it--;
    if (it != m_keyFrames.begin()) it--;
    return it.key();
}

int Layer::getLastKeyFramePosition(int frame) {
    if (m_keyFrames.isEmpty()) return 0;

    keyframe_iterator it = m_keyFrames.upperBound(frame);
    if (it == m_keyFrames.end()) it--;
    if (it != m_keyFrames.begin()) it--;
    return it.key();
}

int Layer::getNextKeyFramePosition(int frame) {
    auto it = m_keyFrames.upperBound(frame);
    if (it == m_keyFrames.end()) it--;
    return it.key();
}

KeyFrame *Layer::getKeyFrameAt(int frame) {
    keyframe_iterator it = m_keyFrames.find(frame);
    if (it == m_keyFrames.end()) {
        return nullptr;
    }
    return it.value();
}

KeyFrame *Layer::getLastKeyFrameAtPosition(int frame) {
    if (m_keyFrames.isEmpty()) return new VectorKeyFrame(this);
    if (frame < 1) frame = 1;
    keyframe_iterator it = m_keyFrames.upperBound(frame);
    if (it == m_keyFrames.end()) it--;
    if (it != m_keyFrames.begin()) it--;
    return it.value();
}

VectorKeyFrame *Layer::getLastVectorKeyFrameAtFrame(int frame, int increment) {
    return static_cast<VectorKeyFrame *>(getLastKeyFrameAtPosition(frame + increment));
}

VectorKeyFrame *Layer::getVectorKeyFrameAtFrame(int frame) { return static_cast<VectorKeyFrame *>(getKeyFrameAt(frame)); }

VectorKeyFrame *Layer::getNextKey(int frame) {
    int nextFrameNumber = getNextKeyFramePosition(frame);
    VectorKeyFrame *nextFrame = getVectorKeyFrameAtFrame(nextFrameNumber);
    return nextFrame;
}

VectorKeyFrame *Layer::getLastKey(int frame) {
    int lastFrameNumber = getLastKeyFramePosition(frame);
    VectorKeyFrame *lastFrame = getVectorKeyFrameAtFrame(lastFrameNumber);
    return lastFrame;
}

VectorKeyFrame *Layer::getPrevKey(int frame) {
    int lastFrameNumber = getPreviousKeyFramePosition(frame);
    VectorKeyFrame *lastFrame = getVectorKeyFrameAtFrame(lastFrameNumber);
    return lastFrame;
}

VectorKeyFrame* Layer::getNextKey(VectorKeyFrame *keyframe) {
    int frame = getVectorKeyFramePosition(keyframe);
    return getNextKey(frame);
}

VectorKeyFrame* Layer::getPrevKey(VectorKeyFrame *keyframe) {
    int frame = getVectorKeyFramePosition(keyframe);
    return getPrevKey(frame);
}

int Layer::getVectorKeyFramePosition(VectorKeyFrame *keyframe) { return m_keyFrames.key(keyframe); }

int Layer::getPreviousFrameNumber(int position, bool keyMode) {
    int prevNumber;

    if (keyMode) {
        keyframe_iterator it = m_keyFrames.upperBound(position);
        if (it != m_keyFrames.constBegin())
            it--;
        else
            return 0;
        if (it == m_keyFrames.constBegin()) return 0;
        it--;

        prevNumber = it.key();
    } else {
        prevNumber = position - 1;
    }

    return prevNumber;
}

int Layer::getNextFrameNumber(int position, bool keyMode) {
    int nextNumber;

    if (keyMode) {
        nextNumber = getNextKeyFramePosition(position);
    } else {
        nextNumber = position + 1;
    }

    return nextNumber;
}

// nb of inbetweens + 1 (t=1.0)
int Layer::stride(int frame) {
    int prev = getLastKeyFramePosition(frame);
    int next = getNextKeyFramePosition(frame);
    return next - prev;
}

int Layer::inbetweenPosition(int frame) {
    int prev = getLastKeyFramePosition(frame);
    return frame - prev;
}

void Layer::insertKeyFrame(int frame, VectorKeyFrame *keyframe) {
    if (m_keyFrames.contains(frame)) {
        delete m_keyFrames[frame];
    }
    m_keyFrames[frame] = keyframe;
}

void Layer::removeKeyFrame(int frame) {
    auto it = m_keyFrames.find(frame);
    if (it != m_keyFrames.end()) {
        delete m_keyFrames[frame];
        it++;
        int next_key = it.key();
        it++;
        if (it == m_keyFrames.end())
            moveKeyFrame(next_key, frame);
        else
            m_keyFrames.remove(frame);
    }
}

void Layer::moveKeyFrame(int oldFrame, int newFrame) {
    VectorKeyFrame *keyframe = m_keyFrames[oldFrame];
    m_keyFrames.remove(oldFrame);
    m_keyFrames[newFrame] = keyframe;
}