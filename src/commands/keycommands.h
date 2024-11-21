#ifndef KEYCOMMANDS_H
#define KEYCOMMANDS_H

#include <QUndoCommand>

#include "vectorkeyframe.h"

class Editor;
class Layer;

class AddKeyCommand : public QUndoCommand {
   public:
    AddKeyCommand(Editor* editor, int layer, int frame, QUndoCommand* parent = nullptr);
    ~AddKeyCommand() override;

    void undo() override;
    void redo() override;

   private:
    Editor* m_editor;
    int m_layer;
    int m_frame_redo;
    int m_frame_undo;
    VectorKeyFrame *m_prevFrameCopy;
};

class AddBreakdownCommand : public QUndoCommand {
   public:
    AddBreakdownCommand(Editor* editor, int layer, int prevFrame, int breakdownFrame, qreal alpha, QUndoCommand* parent = nullptr);
    ~AddBreakdownCommand() override;

    void undo() override;
    void redo() override;

   private:
    Editor* m_editor;
    int m_layer;
    int m_prevFrame;
    int m_breakdownFrame;
    float m_alpha;
    VectorKeyFrame *m_prevFrameCopy;
};

class PasteKeysCommand : public QUndoCommand{
    public:
        PasteKeysCommand(Editor * editor, int layer, int frame, float pivotTranslationFactor = 1., QUndoCommand * parent = nullptr);
        ~PasteKeysCommand() override;

        void undo() override;
        void redo() override;

    private:
        Editor * m_editor;
        QVector<int> m_newKeyFramesIdx;
        QVector<int> m_selectedKeyFramesIdx;
        int m_offset;
        int m_lastFrame;
        int m_layerIndex;
        int m_frame;
        float m_pivotTranslationFactor;
        Point::VectorType m_toPivot;
};

class RemoveKeyCommand : public QUndoCommand {
   public:
    RemoveKeyCommand(Editor* editor, int layer, int frame, QUndoCommand* parent = nullptr);
    ~RemoveKeyCommand() override;

    void undo() override;
    void redo() override;

   private:
    Editor* m_editor;
    int m_layerIndex;
    int m_frame;
    VectorKeyFrame* m_keyframe;
};

class MoveKeyCommand : public QUndoCommand {
   public:
    MoveKeyCommand(Editor* editor, int layer, int startFrame, int endFrame, QUndoCommand* parent = nullptr);
    ~MoveKeyCommand() override;

    void undo() override;
    void redo() override;

   private:
    Editor* m_editor;
    int m_layerIndex;
    int m_startFrame;
    int m_endFrame;
};

/**
 * Creates a correspondence between the given post group the keyframe A and the pre group of the keyframe B 
 */
class SetCorrespondenceCommand : public QUndoCommand {
   public:
    SetCorrespondenceCommand(Editor* editor, int layer, int keyframeA, int keyframeB, int groupA, int groupB, QUndoCommand* parent = nullptr);
    ~SetCorrespondenceCommand() override;

    void undo() override;
    void redo() override;

   private:
    Editor* m_editor;
    int m_layerIndex;
    int m_keyframeA;
    int m_keyframeB;
    int m_groupA;
    int m_groupB;
    int m_prevCorrespondenceCopy;
};

/**
 * Removes the correspondence between the given post group the keyframe A and the pre group of the keyframe B 
 */
class RemoveCorrespondenceCommand : public QUndoCommand {
   public:
    RemoveCorrespondenceCommand(Editor* editor, int layer, int keyframe, int group, QUndoCommand* parent = nullptr);
    ~RemoveCorrespondenceCommand() override;

    void undo() override;
    void redo() override;

   private:
    Editor* m_editor;
    int m_layerIndex;
    int m_keyframe;
    int m_group;
    int m_prevCorrespondenceCopy;
};

class ChangeExposureCommand : public QUndoCommand {
   public:
    ChangeExposureCommand(Editor* editor, int layer, int frame, int exposure, QUndoCommand* parent = nullptr);
    ~ChangeExposureCommand() override;

   private:
    Editor* m_editor;
    int m_layerIndex;
    int m_frame;
    int m_exposure;
};

#endif  // KEYCOMMANDS_H
