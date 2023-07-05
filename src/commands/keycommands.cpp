/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "keycommands.h"
#include "editor.h"
#include "layermanager.h"
#include "layer.h"
#include "tabletcanvas.h"

AddKeyCommand::AddKeyCommand(Editor* editor, int layer, int frame, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_editor(editor)
    , m_layer(layer)
    , m_frame_redo(frame)
    , m_frame_undo(frame)
    , m_prevFrameCopy(nullptr)
{
    setText("Add keyframe");
    Layer *l = m_editor->layers()->layerAt(m_layer); 
    if (m_frame_redo != l->getMaxKeyFramePosition() && l->keyExists(m_frame_redo)) {
        qDebug() << "key exists";
        m_prevFrameCopy = l->getVectorKeyFrameAtFrame(m_frame_redo)->copy();
    }
}

AddKeyCommand::~AddKeyCommand()
{
    delete m_prevFrameCopy;
}

void AddKeyCommand::undo()
{
    if (m_prevFrameCopy != nullptr) {
        m_editor->layers()->layerAt(m_layer)->insertKeyFrame(m_frame_undo, m_prevFrameCopy->copy());
        emit m_editor->tabletCanvas()->frameModified(m_frame_redo);
        m_editor->updateUI(m_editor->layers()->layerAt(m_layer)->getVectorKeyFrameAtFrame(m_frame_undo));
    } else {
        m_editor->removeKeyFrame(m_layer, m_frame_undo);
    }
    m_editor->timelineUpdate(m_frame_undo);
}

void AddKeyCommand::redo()
{
    // If there is already a keyframe, we just clear it, otherwise we add an empty keyframe
    // if (m_prevFrameCopy != nullptr) {
    //     VectorKeyFrame *key = m_editor->layers()->layerAt(m_layer)->getVectorKeyFrameAtFrame(m_frame_redo);
    //     key->clear();
    //     key->makeInbetweensDirty();
    //     emit m_editor->tabletCanvas()->frameModified(m_frame_redo);
    //     m_editor->updateUI(key);
    // } else {
        m_frame_undo = m_editor->addKeyFrame(m_layer, m_frame_redo);
        // TODO: remove all breakdown groups and connected trajectories from the previous group to the next group
        // or maybe just force add a breakdown
    // }
    m_editor->timelineUpdate(m_frame_redo);
}

AddBreakdownCommand::AddBreakdownCommand(Editor* editor, int layer, int prevFrame, int breakdownFrame, float alpha, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_editor(editor)
    , m_layer(layer)
    , m_prevFrame(prevFrame)
    , m_breakdownFrame(breakdownFrame)
    , m_alpha(alpha)
{
    setText("Add breakdown");
}

AddBreakdownCommand::~AddBreakdownCommand()
{
    delete m_prevFrameCopy;
}

void AddBreakdownCommand::undo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    VectorKeyFrame *prevKey = layer->getVectorKeyFrameAtFrame(m_prevFrame);
    VectorKeyFrame *breakdownKey = layer->getVectorKeyFrameAtFrame(m_breakdownFrame);
    int nextFrame = layer->getNextFrameNumber(m_breakdownFrame, true);
    layer->insertKeyFrame(m_prevFrame, m_prevFrameCopy->copy());
    m_editor->removeKeyFrame(m_layer, m_breakdownFrame);
    layer->moveKeyFrame(layer->getNextFrameNumber(m_prevFrame, true), nextFrame);
    m_editor->timelineUpdate(m_breakdownFrame);
    emit m_editor->tabletCanvas()->groupsModified(POST);
    emit m_editor->tabletCanvas()->groupsModified(PRE);
    emit m_editor->tabletCanvas()->groupsModified(MAIN);
}

void AddBreakdownCommand::redo() {
    Layer *layer = m_editor->layers()->layerAt(m_layer);
    int inbetween = layer->inbetweenPosition(m_breakdownFrame);
    VectorKeyFrame *prevKey = layer->getVectorKeyFrameAtFrame(m_prevFrame);
    VectorKeyFrame *nextKey = layer->getNextKey(m_prevFrame);
    Inbetween inbetweenCopy = prevKey->inbetweens()[inbetween - 1];
    m_breakdownFrame = m_editor->addKeyFrame(m_layer, m_breakdownFrame, false);
    VectorKeyFrame *breakdownKey = layer->getVectorKeyFrameAtFrame(m_breakdownFrame);
    m_prevFrameCopy = prevKey->copy();
    prevKey->createBreakdown(m_editor, breakdownKey, nextKey, inbetweenCopy, inbetween, m_alpha);
    m_editor->timelineUpdate(m_breakdownFrame);
    emit m_editor->tabletCanvas()->groupsModified(POST);
    emit m_editor->tabletCanvas()->groupsModified(PRE);
    emit m_editor->tabletCanvas()->groupsModified(MAIN);
}

RemoveKeyCommand::RemoveKeyCommand(Editor* editor, int layer, int frame, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_editor(editor)
    , m_layerIndex(layer)
    , m_frame(frame)
{
    setText("Remove keyframe");
}

RemoveKeyCommand::~RemoveKeyCommand()
{
    delete m_keyframe;
}

void RemoveKeyCommand::undo()
{
    Layer* layer = m_editor->layers()->layerAt(m_layerIndex);
    m_editor->addKeyFrame(m_layerIndex, m_frame);
    if (layer != nullptr) {
        layer->insertKeyFrame(m_frame, m_keyframe->copy());  // restore the image
    }
    m_editor->timelineUpdate(m_frame);
}

void RemoveKeyCommand::redo()
{
    Layer* layer = m_editor->layers()->layerAt(m_layerIndex);
    m_keyframe = layer->getVectorKeyFrameAtFrame(m_frame)->copy();
    layer->removeKeyFrame(m_frame);
    m_editor->timelineUpdate(m_frame);
}

MoveKeyCommand::MoveKeyCommand(Editor *editor, int layer, int startFrame, int endFrame, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_editor(editor)
    , m_layerIndex(layer)
    , m_startFrame(startFrame)
    , m_endFrame(endFrame)
{
    setText("Move keyframe");
}

MoveKeyCommand::~MoveKeyCommand()
{
}

void MoveKeyCommand::undo()
{
    Layer* layer = m_editor->layers()->layerAt(m_layerIndex);
    layer->moveKeyFrame(m_endFrame, m_startFrame);
    m_editor->timelineUpdate(m_startFrame);
}

void MoveKeyCommand::redo()
{
    Layer* layer = m_editor->layers()->layerAt(m_layerIndex);
    layer->moveKeyFrame(m_startFrame, m_endFrame);
    m_editor->timelineUpdate(m_startFrame);
}

SetCorrespondenceCommand::SetCorrespondenceCommand(Editor* editor, int layer, int keyframeA, int keyframeB, int groupA, int groupB, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_editor(editor)
    , m_layerIndex(layer)
    , m_keyframeA(keyframeA)
    , m_keyframeB(keyframeB)
    , m_groupA(groupA)
    , m_groupB(groupB)
{
    setText("Set correspondence");

    // save the previous correspondence if there is any
    Layer* lay = m_editor->layers()->layerAt(m_layerIndex);
    VectorKeyFrame *keyA = lay->getVectorKeyFrameAtFrame(m_keyframeA);
    if (keyA->correspondences().contains(m_groupA)) {
        m_prevCorrespondenceCopy = keyA->correspondences().value(m_groupA);
    } else {
        m_prevCorrespondenceCopy = Group::ERROR_ID;
    }
}

SetCorrespondenceCommand::~SetCorrespondenceCommand()
{

}

void SetCorrespondenceCommand::undo()
{
    Layer* layer = m_editor->layers()->layerAt(m_layerIndex);
    VectorKeyFrame *keyframeA = layer->getVectorKeyFrameAtFrame(m_keyframeA);
    VectorKeyFrame *keyframeB = layer->getVectorKeyFrameAtFrame(m_keyframeB);
    keyframeA->removeCorrespondence(m_groupA);
    if (m_prevCorrespondenceCopy != Group::ERROR_ID) {
        keyframeA->addCorrespondence(m_groupA, m_prevCorrespondenceCopy);
    }
    Group *groupA = keyframeA->postGroups().fromId(m_groupA);
    groupA->lattice()->setBackwardUVDirty(true);
    keyframeA->makeInbetweensDirty();
}

void SetCorrespondenceCommand::redo()
{
    Layer* layer = m_editor->layers()->layerAt(m_layerIndex);
    VectorKeyFrame *keyframeA = layer->getVectorKeyFrameAtFrame(m_keyframeA);
    VectorKeyFrame *keyframeB = layer->getVectorKeyFrameAtFrame(m_keyframeB);
    Group *groupA = keyframeA->postGroups().fromId(m_groupA);  
    Group *groupB = keyframeB->preGroups().fromId(m_groupB);  
    keyframeA->addCorrespondence(m_groupA, m_groupB);
    groupA->lattice()->setBackwardUVDirty(true);
    keyframeA->makeInbetweensDirty();
}

RemoveCorrespondenceCommand::RemoveCorrespondenceCommand(Editor* editor, int layer, int keyframe, int group, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_editor(editor)
    , m_layerIndex(layer)
    , m_keyframe(keyframe)
    , m_group(group)
{
    setText("Remove correspondence");

    // save the previous correspondence if there is any
    Layer* lay = m_editor->layers()->layerAt(m_layerIndex);
    VectorKeyFrame *keyA = lay->getVectorKeyFrameAtFrame(m_keyframe);
    if (keyA->correspondences().contains(m_group)) {
        m_prevCorrespondenceCopy = keyA->correspondences().value(m_group);
    } else {
        m_prevCorrespondenceCopy = Group::ERROR_ID;
    }
}

RemoveCorrespondenceCommand::~RemoveCorrespondenceCommand()
{

}

void RemoveCorrespondenceCommand::undo()
{
    Layer* layer = m_editor->layers()->layerAt(m_layerIndex);
    VectorKeyFrame *keyframe = layer->getVectorKeyFrameAtFrame(m_keyframe);
    if (m_prevCorrespondenceCopy != Group::ERROR_ID) {
        keyframe->addCorrespondence(m_group, m_prevCorrespondenceCopy);
    }
    Group *group = keyframe->postGroups().fromId(m_group);
    group->lattice()->setBackwardUVDirty(true);
    keyframe->makeInbetweensDirty();
}

void RemoveCorrespondenceCommand::redo()
{
    Layer* layer = m_editor->layers()->layerAt(m_layerIndex);
    VectorKeyFrame *keyframe = layer->getVectorKeyFrameAtFrame(m_keyframe);
    Group *group = keyframe->postGroups().fromId(m_group);  
    keyframe->removeCorrespondence(m_group);
    group->lattice()->setBackwardUVDirty(true);
    keyframe->makeInbetweensDirty();
    // TODO delete pre group?
}

ChangeExposureCommand::ChangeExposureCommand(Editor *editor, int layerIndex, int frame, int exposure, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_editor(editor)
    , m_layerIndex(layerIndex)
    , m_frame(frame)
    , m_exposure(exposure)
{
    setText("Change exposure");

    Layer* layer = m_editor->layers()->layerAt(m_layerIndex);
    QList<int> keyFrameIndices = layer->keys();
    for(int i = 0; i < keyFrameIndices.size(); i++) {
        int key = keyFrameIndices[i];
        if(key > m_frame)
            new MoveKeyCommand(m_editor, m_layerIndex, key, key + m_exposure, this);
    }
}

ChangeExposureCommand::~ChangeExposureCommand()
{
}
