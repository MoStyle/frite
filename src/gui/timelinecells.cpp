/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "timelinecells.h"
#include "timeline.h"
#include "layer.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "stylemanager.h"
#include "layercommands.h"
#include "tabletcanvas.h"
#include "editor.h"
#include "keycommands.h"

#include <QtWidgets>


TimeLineCells::TimeLineCells(TimeLine* parent, Editor* editor, TimeLineCellsTypes type)
    : QWidget(parent), timeLine(parent), m_editor(editor), type(type), m_isChangingOpacity(false), m_selectionBox(QRubberBand::Rectangle, this)
{
    cache = nullptr;
    QSettings settings("manao","Frite");

    frameLength = settings.value("length").toInt();
    if (frameLength==0) {
        frameLength=240;
        settings.setValue("length", frameLength);
    }

    startY = 0;
    endY = 0;
    mouseMoveY = 0;
    m_startLayerNumber = -1;
    offsetX = 0;
    offsetY = 25;
    frameOffset = 0;
    layerOffset = 0;

    frameSize = (settings.value("frameSize").toInt());
    if (frameSize==0) {
        frameSize=16;
        settings.setValue("frameSize", frameSize);
    }

    fontSize = (settings.value("labelFontSize").toInt());
    if (fontSize==0) {
        fontSize=10;
        settings.setValue("labelFontSize", fontSize);
    }

    layerHeight = (settings.value("layerHeight").toInt());
    if(layerHeight==0) {
        layerHeight=20;
        settings.setValue("layerHeight", layerHeight);
    }

    setMinimumSize(500, 4*layerHeight);
    setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding));
    setAttribute(Qt::WA_OpaquePaintEvent, false);    
}

TimeLineCells::~TimeLineCells()
{
    if (cache) delete cache;
}

int TimeLineCells::getFrameNumber(int x)
{
    int frameNumber = frameOffset+1+(x-offsetX)/frameSize;
    return frameNumber;
}

int TimeLineCells::getFrameX(int frameNumber)
{
    int x = offsetX + (frameNumber-frameOffset)*frameSize;
    return x;
}

int TimeLineCells::getLayerNumber(int y)
{
    int layerCount = m_editor->layers()->layersCount();
    int layerNumber = layerOffset + (y-offsetY)/layerHeight;
    layerNumber = layerCount-1-layerNumber;
    if (y < offsetY)
        layerNumber = -1;

    if (layerNumber > layerCount)
        layerNumber = layerCount;

    return layerNumber;
}

int TimeLineCells::getLayerY(int layerNumber)
{
    return offsetY + (m_editor->layers()->layersCount()-1-layerNumber-layerOffset)*layerHeight;
}

bool TimeLineCells::selectionContainsVectorKeyFrame(int frame){
    if (!m_selectionBox.isVisible()) return false;
    int currentLayer = m_editor->layers()->currentLayerIndex();
    QRect rect = m_selectionBox.geometry();
    if (rect.bottomLeft().y() < getLayerY(currentLayer) || rect.topLeft().y() > getLayerY(currentLayer) + getLayerHeight())
        return false;
        
    Layer * layer = m_editor->layers()->layerAt(currentLayer);
    int frameMin = getFrameNumber(rect.topLeft().x());
    if (frameMin >= layer->getMaxKeyFramePosition())
        return false;

    frameMin = layer->getLastKeyFramePosition(frameMin);
    int frameMax = getFrameNumber(rect.topRight().x());
    frameMax = layer->getLastKeyFramePosition(frameMax);
    return frameMin <= frame && frame <= frameMax;
}

void TimeLineCells::updateFrame(int frameNumber)
{
    int x = getFrameX(frameNumber);
    update(x-frameSize,0,frameSize+1,height());
}

void TimeLineCells::updateContent()
{
    drawContent();
    update();
}

void TimeLineCells::drawContent()
{
    if (cache == nullptr) {
        cache = new QPixmap(size());
    }
    if (cache->isNull()) return;

    QPainter painter(cache);

    // grey background of the view
    painter.setPen(Qt::NoPen);
    painter.setBrush(QGuiApplication::palette().color(QPalette::Window));
    painter.drawRect(QRect(0,0, width(), height()));

    Layer* layer = m_editor->layers()->currentLayer();
    if(layer) {
        for (int i = 0; i < m_editor->layers()->layersCount(); i++) {
            if (i != m_editor->layers()->currentLayerIndex()) {
                Layer* layeri = m_editor->layers()->layerAt(i);
                if(type == TYPE_TRACKS)
                    layeri->paintTrack(painter, this, offsetX, getLayerY(i), width()-offsetX, false);
                if(type == TYPE_LAYER_ATTR)
                    layeri->paintLabel(painter, 0, getLayerY(i), width()-1, getLayerHeight(), false);
            } else {
                if (abs(getMouseMoveY()) > 5) {
                    if (type == TYPE_TRACKS)
                        layer->paintTrack(painter, this, offsetX, getLayerY(m_editor->layers()->currentLayerIndex()) + getMouseMoveY(), width() - offsetX, true);
                    if (type == TYPE_LAYER_ATTR)
                        layer->paintLabel(painter, 0, getLayerY(m_editor->layers()->currentLayerIndex()) + getMouseMoveY(), width() - 1, getLayerHeight(), true);
                    painter.setPen(Qt::black);
                    painter.drawRect(0, getLayerY(getLayerNumber(endY)) -1, width(), 2);
                } else {
                    if(type == TYPE_TRACKS)
                        layer->paintTrack(painter, this, offsetX, getLayerY(m_editor->layers()->currentLayerIndex()), width()-offsetX, true);
                    if(type == TYPE_LAYER_ATTR)
                        layer->paintLabel(painter, 0, getLayerY(m_editor->layers()->currentLayerIndex()), width()-1, getLayerHeight(), true);
                }
            }
        }
    }

    // --- draw top
    painter.setPen(Qt::NoPen);
    painter.setBrush(QGuiApplication::palette().color(QPalette::Window));
    painter.drawRect(QRect(0,0, width()-1, offsetY-1));
    painter.setPen(Qt::gray);
    painter.drawLine(0,0, width()-1, 0);
    painter.drawLine(0,offsetY-2, width()-1, offsetY-2);
    painter.setPen(Qt::lightGray);
    painter.drawLine(0,offsetY-3, width()-1, offsetY-3);
    painter.drawLine(0,0, 0, offsetY-3);

    if(type == TYPE_LAYER_ATTR) {
        // --- draw circle
        painter.setPen(QGuiApplication::palette().color(QPalette::WindowText));
        painter.setRenderHint(QPainter::Antialiasing, true);
        QRect targetRect = QRect(2, 3, 16, 16);
        painter.drawPixmap(targetRect, QPixmap(m_editor->style()->getResourcePath("eye")));

        // --- draw onion
        targetRect = QRect(20, 3, 16, 16);
        painter.drawPixmap(targetRect, QPixmap(m_editor->style()->getResourcePath("onionOn")));
        painter.setRenderHint(QPainter::Antialiasing, false);

        // --- draw mask
        targetRect = QRect(38, 3, 16, 16);
        painter.drawPixmap(targetRect, QPixmap(m_editor->style()->getResourcePath("mask")));
        painter.setRenderHint(QPainter::Antialiasing, false);
    }

    if (type == TYPE_TRACKS) {
        // --- draw ticks
        painter.setPen(QGuiApplication::palette().color(QPalette::Midlight));
        painter.setBrush(QGuiApplication::palette().color(QPalette::WindowText));
        QFont f = QApplication::font();
        f.setPointSize(fontSize);
        painter.setFont(f);
        int incr = 0;
        int fps = 24;
        for (int i = frameOffset; i < frameOffset + (width() - offsetX) / frameSize; i++) {
            incr = (i < 10) ? 4 : 2;

            if (i%fps == fps-1) {
                painter.drawLine(getFrameX(i), 10, getFrameX(i), offsetY-4);
            } else if (i%fps == fps / 2 - 1) {
                painter.drawLine(getFrameX(i), 14, getFrameX(i), offsetY-4);
            } else {
                painter.drawLine(getFrameX(i), 16, getFrameX(i), offsetY-4);
            }
            if (i == 0 || i%fps == fps - 1 || i%fps == fps / 2 - 1) {
                painter.setPen(QGuiApplication::palette().color(QPalette::WindowText));
                painter.drawText(QPoint(getFrameX(i) + incr, 18), QString::number(i + 1));
                painter.setPen(QGuiApplication::palette().color(QPalette::Midlight));
            }

        }

        // --- draw left border line
        painter.drawLine(0,0, 0, height());
    }
}

void TimeLineCells::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);

    bool isPlaying = m_editor->playback()->isPlaying();
    if ((!isPlaying && !timeLine->scrubbing) || cache == nullptr)
    {
        drawContent();
    }
    if (cache) {
        painter.drawPixmap(QPoint(0, 0), *cache);
    }

    if (type == TYPE_TRACKS) {
        //--- draw the position of the current frame
        int currentFrameIndex = m_editor->playback()->currentFrame();
        if(currentFrameIndex > frameOffset) {
            QColor c = QGuiApplication::palette().color(QPalette::Highlight);
            c.setAlpha(128);
            painter.setBrush(c);
            painter.setPen(c);
            QFont f = QApplication::font();
            f.setPointSize(fontSize);
            painter.setFont(f);
            QRect scrubRect;
            scrubRect.setTopLeft(QPoint(getFrameX(currentFrameIndex - 1), 5));
            scrubRect.setBottomRight(QPoint(getFrameX(currentFrameIndex)-1, offsetY-4));
            painter.drawRect(scrubRect);

            painter.setBrush(Qt::NoBrush);
            painter.drawLine(getFrameX(currentFrameIndex - 1), 1, getFrameX(currentFrameIndex - 1), height());
            painter.drawLine(getFrameX(currentFrameIndex), 1, getFrameX(currentFrameIndex), height());

            painter.setPen(QGuiApplication::palette().color(QPalette::Text));
            int incr = (currentFrameIndex < 10) ? 4 : 2;
            painter.drawText(QPoint(getFrameX(currentFrameIndex - 1) + incr, 18),
                              QString::number(currentFrameIndex));
        }
    }
}

void TimeLineCells::resizeEvent(QResizeEvent* event)
{
    if(cache) delete cache;
    cache = new QPixmap(size());
    updateContent();
    event->accept();
    emit lengthChanged(getFrameLength());
}

void TimeLineCells::mousePressEvent(QMouseEvent* event)
{
    int frameNumber = getFrameNumber(event->pos().x());
    int layerNumber = getLayerNumber(event->pos().y());

    startY = event->pos().y();
    m_startLayerNumber = layerNumber;
    endY = event->pos().y();
    int currentFrameIndex = m_editor->playback()->currentFrame();

    if (event->modifiers() & Qt::ControlModifier){
        int currentLayer = m_editor->layers()->currentLayerIndex();
        if (!(event->modifiers() & Qt::ShiftModifier))
            m_editor->layers()->layerAt(currentLayer)->clearSelectedKeyFrame();
        m_selectionBoxOrigin = event->pos();
        m_selectionBox.setGeometry(QRect(m_selectionBoxOrigin, m_selectionBoxOrigin));
        m_selectionBox.show();
    } else if (type == TYPE_LAYER_ATTR) {
        if (layerNumber >= 0 && layerNumber < m_editor->layers()->layersCount()) {
            if (event->pos().x() < 22) {
                m_editor->undoStack()->push(new SwitchVisibilityCommand(m_editor->layers(), layerNumber));
            } else if(event->pos().x() < 37) {
                m_editor->undoStack()->push(new SwitchOnionCommand(m_editor->layers(), layerNumber));
            } else if (event->pos().x() < 55) {
                m_editor->undoStack()->push(new SwitchHasMaskCommand(m_editor->layers(), layerNumber));
            } else if(event->pos().x() > 150 && event->pos().x() < 185) {
                m_prevOpacity = qreal(event->pos().x()-150)/35.;
                m_isChangingOpacity = true;
            } else {
                m_editor->layers()->setCurrentLayer(layerNumber);
            }
        }
        if (layerNumber == -1) {
            if (event->pos().x() < 22) {
                for(int l=0; l<m_editor->layers()->layersCount(); l++)
                    m_editor->undoStack()->push(new SwitchVisibilityCommand(m_editor->layers(), l));
            } else if(event->pos().x() < 37) {
                for(int l=0; l<m_editor->layers()->layersCount(); l++)
                    m_editor->undoStack()->push(new SwitchOnionCommand(m_editor->layers(), l));
            }
        }
    } else if (type == TYPE_TRACKS) {
        if (frameNumber == currentFrameIndex && startY < 20) {
            if (m_editor->playback()->isPlaying())
            {
                m_editor->playback()->stop();
            }
            timeLine->scrubbing = true;
        } else {
            if (layerNumber >= 0 && layerNumber < m_editor->layers()->layersCount()) {
                int previousLayerNumber = m_editor->layers()->currentLayerIndex();

                if (previousLayerNumber != layerNumber) {
                    Layer *previousLayer = m_editor->layers()->layerAt(previousLayerNumber);
                    previousLayer->deselectAllKeys();

                    m_editor->setCurrentLayer(layerNumber);
                }
                Layer* layer = m_editor->layers()->layerAt(layerNumber);
                layer->startMoveKeyframe(this, event, frameNumber, getLayerY(layerNumber));
                updateContent();
            } else {
                if(frameNumber > 0) {
                    emit currentFrameChanged(frameNumber);
                    timeLine->scrubbing = true;
                }
            }
        }
    }
    timeLine->updateContent();
}

void TimeLineCells::mouseMoveEvent(QMouseEvent* event)
{
    int frameNumber = getFrameNumber(event->pos().x());
    int layerNumber = getLayerNumber(event->pos().y());

    if (event->modifiers() & Qt::ControlModifier){
        m_selectionBox.setGeometry(QRect(m_selectionBoxOrigin, event->pos()).normalized());
    }

    if (type == TYPE_LAYER_ATTR) {
        if(m_isChangingOpacity) {
            Layer* layer = m_editor->layers()->layerAt(m_startLayerNumber);
            if (event->pos().x() <= 150)
                layer->setOpacity(0);
            else if (event->pos().x() >= 185)
                layer->setOpacity(1);
            else
                layer->setOpacity(qreal(event->pos().x()-150)/35.);
            m_editor->tabletCanvas()->updateCurrentFrame();
        } else {
            endY = event->pos().y();
            emit mouseMovedY(endY - startY);
        }
    }



    if (type == TYPE_TRACKS && frameNumber>0) {
        if (timeLine->scrubbing) {
            m_editor->playback()->setPlaying(true);
            emit currentFrameChanged(frameNumber);
        } else {
            if (m_startLayerNumber >= 0 && m_startLayerNumber < m_editor->layers()->layersCount()) {
                m_editor->layers()->layerAt(m_startLayerNumber)->moveKeyframe(event, frameNumber);
            }
        }
    }
    timeLine->updateContent();
}

void TimeLineCells::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_selectionBox.isVisible())
        m_selectionBox.hide();

    if (event->modifiers() & Qt::ControlModifier){
        QRect rect = m_selectionBox.geometry();
        int currentLayer = m_editor->layers()->currentLayerIndex();
        Layer * layer = m_editor->layers()->layerAt(currentLayer);
        if (rect.bottomLeft().y() > getLayerY(currentLayer) && rect.topLeft().y() < getLayerY(currentLayer) + getLayerHeight()){
            int layerMin = layer->getFirstKeyFrameSelected();
            int layerMax = layer->getLastKeyFrameSelected();
            int frameMin = getFrameNumber(rect.topLeft().x());
            int frameMax = getFrameNumber(rect.topRight().x());
            if (event->modifiers() & Qt::ShiftModifier && !layer->selectedKeyFrameIsEmpty()){
                if (frameMax < layerMin){
                    for (int frame = frameMax; frame < layerMin; frame++){
                        layer->addSelectedKeyFrame(frame);
                    }
                }
                if (frameMin > layerMax){
                    for(int frame = layerMax; frame < frameMin; frame++){
                        layer->addSelectedKeyFrame(frame);
                    }
                }
            }
            for (int x = rect.topLeft().x(); x <= rect.topRight().x(); x += frameSize / 2){
                layer->addSelectedKeyFrame(getFrameNumber(x));
            }
            layer->sortSelectedKeyFrames();
        }
        timeLine->update();
        return;
    }
    
    m_editor->playback()->setPlaying(false);
    endY = startY;
    emit mouseMovedY(0);
    timeLine->scrubbing = false;
    if(m_isChangingOpacity) {
        Layer* layer = m_editor->layers()->layerAt(m_startLayerNumber);
        layer->setOpacity(m_prevOpacity);
        if (event->pos().x() <= 150) {
            m_editor->undoStack()->push(new ChangeOpacityCommand(m_editor->layers(), m_startLayerNumber, 0));
        } else if(event->pos().x() >= 185) {
            m_editor->undoStack()->push(new ChangeOpacityCommand(m_editor->layers(), m_startLayerNumber, 1));
        } else {
            m_editor->undoStack()->push(new ChangeOpacityCommand(m_editor->layers(), m_startLayerNumber, qreal(event->pos().x()-150)/35.));
        }
        m_isChangingOpacity = false;
    }
    int frameNumber = getFrameNumber(event->pos().x());
    if(frameNumber < 1) frameNumber = -1;
    int layerNumber = getLayerNumber(event->pos().y());

    if(type == TYPE_TRACKS && m_startLayerNumber >=0 && layerNumber < m_editor->layers()->layersCount() && frameNumber>0) {
        m_editor->layers()->layerAt(m_startLayerNumber)->stopMoveKeyframe(event, m_startLayerNumber, frameNumber);
    } else if(type == TYPE_LAYER_ATTR && layerNumber != m_startLayerNumber && m_startLayerNumber != -1 && layerNumber != -1) {
        m_editor->undoStack()->push(new MoveLayerCommand(m_editor->layers(), m_startLayerNumber, layerNumber));
        m_editor->tabletCanvas()->update();
    }
    emit currentFrameChanged(frameNumber);
    timeLine->updateContent();
}

void TimeLineCells::mouseDoubleClickEvent(QMouseEvent* event)
{
    int layerNumber = getLayerNumber(event->pos().y());
    if(layerNumber<0)
        return;
    Layer* layer = m_editor->layers()->layerAt(layerNumber);

    if (event->modifiers() & Qt::ControlModifier){
        layer->clearSelectedKeyFrame();
        timeLine->updateContent();
        return;
    }

    // -- layer --
    if(type == TYPE_LAYER_ATTR && event->pos().x() >= 15) {
        bool ok;
        QString text = QInputDialog::getText(nullptr, QInputDialog::tr("Layer Properties"),
                                             QInputDialog::tr("Layer name:"), QLineEdit::Normal,
                                             layer->name(), &ok);
        if (ok && !text.isEmpty())
            layer->setName(text);
        update();
    }
}

void TimeLineCells::contextMenuEvent(QContextMenuEvent *event)
{
    if (type == TYPE_TRACKS) {
        m_startLayerNumber = getLayerNumber(event->pos().y());
        if(m_startLayerNumber<0)
            return;
        Layer* layer = m_editor->layers()->layerAt(m_startLayerNumber);
        m_frameNumber = getFrameNumber(event->pos().x());
        QMenu contextMenu(this);
        if (!layer->selectedKeyFrameIsEmpty()){
            contextMenu.addAction(tr("Register from rest position"), this, &TimeLineCells::automaticRegistration);
        }
        contextMenu.addSeparator();
        if (!layer->selectedKeyFrameIsEmpty()){
            QMenu * subMenuPaste = contextMenu.addMenu("Paste Key Frames ...");
            subMenuPaste->addAction(tr("at this frame"), this, &TimeLineCells::pasteKeyFrame);
            subMenuPaste->addAction(tr("at the end"), this, &TimeLineCells::pasteKeyFrameAtTheEnd);
            QMenu * subMenuLoop = contextMenu.addMenu("Loop KeyFrames ...");
            subMenuLoop->addAction(tr("at this frame"), this, &TimeLineCells::pasteMultipleKeyFrame);
            subMenuLoop->addAction(tr("at the end"), this, &TimeLineCells::pasteMultipleKeyFrameAtTheEnd);
        }
        contextMenu.addSeparator();
        if(layer->getMaxKeyFramePosition() > m_frameNumber){
            contextMenu.addAction(tr("Set Exposure Value..."), this, &TimeLineCells::changeExposure);
            contextMenu.addAction(tr("Delete image..."), this, &TimeLineCells::deleteImage);
        }
        contextMenu.exec(event->globalPos());
    }
}

void TimeLineCells::changeExposure()
{
    Layer* layer = m_editor->layers()->layerAt(m_startLayerNumber);
    int keyIndex = m_frameNumber;
    if(!layer->keyExists(m_frameNumber))
        m_frameNumber = layer->getPreviousKeyFramePosition(m_frameNumber);
    int old_exposure = layer->getNextKeyFramePosition(m_frameNumber) - keyIndex ;
    bool ok;
    int new_exposure = QInputDialog::getInt(this, tr("New exposure"), tr("Exposure"), old_exposure, 1, 2147483647, 1, &ok);
    if (ok && new_exposure!=old_exposure)
        m_editor->undoStack()->push(new ChangeExposureCommand(m_editor, m_startLayerNumber, m_frameNumber, new_exposure - old_exposure));
}

void TimeLineCells::deleteImage()
{
    m_editor->undoStack()->push(new ChangeExposureCommand(m_editor, m_startLayerNumber, m_frameNumber, -1));
}

void TimeLineCells::automaticRegistration(){
    Layer * layer = m_editor->layers()->layerAt(m_startLayerNumber);
    for (VectorKeyFrame * key : layer->getSelectedKeyFrames()){
        m_editor->registerFromRestPosition(key, true);
    }
}

void TimeLineCells::pasteKeyFrame(){
    Layer * layer = m_editor->layers()->layerAt(m_startLayerNumber);
    int keyIndex = m_frameNumber;
    layer->insertSelectedKeyFrame(m_startLayerNumber, keyIndex);
    update();
}

void TimeLineCells::pasteKeyFrameAtTheEnd(){
    Layer * layer = m_editor->layers()->layerAt(m_startLayerNumber);
    int keyIndex = layer->getMaxKeyFramePosition();
    layer->insertSelectedKeyFrame(m_startLayerNumber, keyIndex);
    update();
}

void TimeLineCells::pasteMultipleKeyFrame(){
    Layer * layer = m_editor->layers()->layerAt(m_startLayerNumber);
    int keyIndex = m_frameNumber;
    bool ok;
    int n = QInputDialog::getInt(this, tr("Add loops"), tr("Number"), 1, 1, 100, 1, &ok);
    if (!ok) return;
    layer->insertSelectedKeyFrame(m_startLayerNumber, keyIndex, n);
    update();
}

void TimeLineCells::pasteMultipleKeyFrameAtTheEnd(){
    Layer * layer = m_editor->layers()->layerAt(m_startLayerNumber);
    int keyIndex = layer->getMaxKeyFramePosition();
    bool ok;
    int n = QInputDialog::getInt(this, tr("Add loops"), tr("Number"), 1, 1, 100, 1, &ok);
    if (!ok) return;
    layer->insertSelectedKeyFrame(m_startLayerNumber, keyIndex, n);
    update();
}


void TimeLineCells::fontSizeChange(int x)
{
    fontSize=x;
    QSettings settings("manao","Frite");
    settings.setValue("labelFontSize", x);
    updateContent();
}

void TimeLineCells::frameSizeChange(int x)
{
    frameSize = x;
    QSettings settings("manao","Frite");
    settings.setValue("frameSize", x);
    updateContent();
}

void TimeLineCells::labelChange(int x)
{
    QSettings settings("manao","Frite");
    if (x==0) {
        drawFrameNumber=false;
        settings.setValue("drawLabel","false");
    } else {
        drawFrameNumber=true;
        settings.setValue("drawLabel","true");
    }
    updateContent();
}

void TimeLineCells::hScrollChange(int x)
{
    frameOffset = x;
    updateContent();
}

void TimeLineCells::vScrollChange(int x)
{
    layerOffset = x;
    updateContent();
}
