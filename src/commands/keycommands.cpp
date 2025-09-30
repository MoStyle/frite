/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
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

AddBreakdownCommand::AddBreakdownCommand(Editor* editor, int layer, int prevFrame, int breakdownFrame, qreal alpha, QUndoCommand *parent)
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
    Inbetween inbetweenCopy = prevKey->inbetweens()[inbetween];
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

PasteKeysCommand::PasteKeysCommand(Editor * editor, int layer, int frame, float pivotTranslationFactor, QUndoCommand * parent)
    : QUndoCommand(parent)
    , m_editor(editor)
    , m_layerIndex(layer)
    , m_frame(frame)
    , m_pivotTranslationFactor(pivotTranslationFactor)
    , m_toPivot(0, 0)
{
    Layer * l = m_editor->layers()->layerAt(m_layerIndex);
    m_lastFrame = l->getMaxKeyFramePosition();
    for (VectorKeyFrame * initialKey : l->getSelectedKeyFrames()){
        int initialKeyIdx = l->getVectorKeyFramePosition(initialKey);
        m_selectedKeyFramesIdx.append(initialKeyIdx);
    }
    setText("Paste keyFrames");
}
PasteKeysCommand::~PasteKeysCommand(){
}

void PasteKeysCommand::undo(){
    Layer * layer = m_editor->layers()->layerAt(m_layerIndex);
    for (int frame : m_newKeyFramesIdx){
        layer->removeKeyFrame(frame);
        m_editor->timelineUpdate(frame);
    }
    m_newKeyFramesIdx.clear();

    if (layer->getMaxKeyFramePosition() != m_lastFrame){
        QList<int> keyFrameIndices = layer->keys();
        bool sign = m_offset >= 0;
        auto moveKeyFrames = [&](int &key){if(key >= m_frame) layer->moveKeyFrame(key, key - m_offset); };
        if (sign)
            std::for_each(keyFrameIndices.begin(), keyFrameIndices.end(), moveKeyFrames);
        else
            std::for_each(keyFrameIndices.rbegin(), keyFrameIndices.rend(), moveKeyFrames);
        m_editor->timelineUpdate(m_lastFrame);
    }
}
void PasteKeysCommand::redo(){
    Layer * layer = m_editor->layers()->layerAt(m_layerIndex);

    // VectorKeyFrame * lastSelected = layer->getVectorKeyFrameAtFrame(*(m_selectedKeyFramesIdx.end() - 1));
    // KeyframedVector * translation = lastSelected->translation();
    // translation->frameChanged(1);
    int nextFrame = layer->getNextKeyFramePosition(*(m_selectedKeyFramesIdx.end() - 1));
    m_toPivot = layer->getPivotPosition(nextFrame);
    // VectorKeyFrame * firstSelected = layer->getVectorKeyFrameAtFrame(*m_selectedKeyFramesIdx.begin());
    // translation = firstSelected->translation();
    // translation->frameChanged(0);
    // m_toPivot -= translation->get();
    m_toPivot -= layer->getPivotPosition(*(m_selectedKeyFramesIdx.begin()));
    m_toPivot *= m_pivotTranslationFactor;    

    if (layer->getMaxKeyFramePosition() <= m_frame){
        int offset = 0;
        bool first = true;
        for (int initialKeyIdx : m_selectedKeyFramesIdx){
            VectorKeyFrame * initialKey = layer->getVectorKeyFrameAtFrame(initialKeyIdx);
            int lastKeyFramePosition = layer->getMaxKeyFramePosition();

            // added empty keyFrame if newFrame doesn't corrospond to lastKeyFrame
            if (lastKeyFramePosition != m_frame + offset){
                layer->moveKeyFrame(lastKeyFramePosition, lastKeyFramePosition + 1);
                layer->addNewEmptyKeyAt(lastKeyFramePosition);
                m_newKeyFramesIdx.prepend(lastKeyFramePosition);
                lastKeyFramePosition++;
            }
            int frame = m_frame + offset;
            layer->moveKeyFrame(lastKeyFramePosition, frame + layer->stride(initialKeyIdx));
            layer->insertKeyFrame(frame, initialKey->copy());

            if (initialKey->isTranslationExtracted()){
                // pivot translation
                Point::VectorType point = layer->getPivotControlPoint(initialKeyIdx);
                layer->addPointToPivotCurve(frame, point);
                layer->getVectorKeyFrameAtFrame(frame)->setPivotCurve(layer->getPivotCurves()->getBezier(layer->getFrameTValue(frame)));

                layer->addVectorKeyFrameTranslation(frame, m_toPivot, !first);
            }

            m_newKeyFramesIdx.prepend(frame);
            offset += layer->stride(initialKeyIdx);
            first = false;
        }
        int lastKeyFramePosition = layer->getMaxKeyFramePosition();
        // TODO : find a more convenient way to move the lastKeyFrame position and update its previous keyframe pivot
        layer->addVectorKeyFrameTranslation(lastKeyFramePosition, m_toPivot);
        layer->addVectorKeyFrameTranslation(lastKeyFramePosition, -m_toPivot, false);
        layer->addVectorKeyFrameTranslation(lastKeyFramePosition, m_toPivot / m_pivotTranslationFactor, false);

        m_offset = offset;
    }
    else{
        int offset = 0;
        std::vector<VectorKeyFrame *> initialKeyFrames;
        if (!layer->keyExists(m_frame))
            offset = -(layer->getNextKeyFramePosition(m_frame) - m_frame);
        for (int initialKeyIdx : m_selectedKeyFramesIdx){
            VectorKeyFrame * keyFrame = layer->getVectorKeyFrameAtFrame(initialKeyIdx);
            initialKeyFrames.push_back(keyFrame);
            offset += layer->stride(layer->getVectorKeyFramePosition(keyFrame)); 
        }
        m_offset = offset;

        QList<int> keyFrameIndices = layer->keys();
        bool sign = m_offset >= 0;
        auto moveKeyFrames = [&](int &key){ if(key >= m_frame) layer->moveKeyFrame(key, key + offset); };
        if (sign)
            std::for_each(keyFrameIndices.rbegin(), keyFrameIndices.rend(), moveKeyFrames);
        else
            std::for_each(keyFrameIndices.begin(), keyFrameIndices.end(), moveKeyFrames);

        offset = 0;
        for(VectorKeyFrame * initialKeyFrame : initialKeyFrames){
            layer->insertKeyFrame(m_frame + offset, initialKeyFrame->copy());
            m_newKeyFramesIdx.prepend(m_frame + offset);
            offset += layer->stride(layer->getVectorKeyFramePosition(initialKeyFrame));
        }
    }
    for (auto it = layer->keysBegin(); it != layer->keysEnd(); ++it){
        int frame = it.key();
        layer->getVectorKeyFrameAtFrame(frame)->updateTransforms();
    }
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
