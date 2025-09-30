/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef LAYERCOMMANDS_H
#define LAYERCOMMANDS_H

#include <QUndoCommand>

class LayerManager;
class Layer;
class Editor;

class AddLayerCommand : public QUndoCommand {
public:
  AddLayerCommand(LayerManager *layerManager, int layer,
                  QUndoCommand *parent = nullptr);
  ~AddLayerCommand() override;

  void undo() override;
  void redo() override;

private:
  LayerManager *m_layerManager;
  QString m_layerName;
  int m_layerIndex;
};

class RemoveLayerCommand : public QUndoCommand {
public:
  RemoveLayerCommand(LayerManager *layerManager, int layerIndex,
                     QUndoCommand *parent = nullptr);
  ~RemoveLayerCommand() override;

  void undo() override;
  void redo() override;

private:
  LayerManager *m_layerManager;
  QString m_layerName;
  int m_layerIndex;
};

class MoveLayerCommand : public QUndoCommand {
public:
  MoveLayerCommand(LayerManager *layerManager, int layerIndex1, int layerIndex2,
                   QUndoCommand *parent = nullptr);
  ~MoveLayerCommand() override;

  void undo() override;
  void redo() override;

private:
  LayerManager *m_layerManager;
  int m_layerIndex1;
  int m_layerIndex2;
};

class ChangeOpacityCommand : public QUndoCommand {
public:
  ChangeOpacityCommand(LayerManager *layerManager, int layer, qreal opacity,
                       QUndoCommand *parent = nullptr);
  ~ChangeOpacityCommand() override;

  void undo() override;
  void redo() override;

private:
  LayerManager *m_layerManager;
  int m_layerIndex;
  qreal m_opacity;
  qreal m_prevOpacity;
};

class SwitchVisibilityCommand : public QUndoCommand {
public:
  SwitchVisibilityCommand(LayerManager *layerManager, int layer,
                          QUndoCommand *parent = nullptr);
  ~SwitchVisibilityCommand() override;

  void undo() override;
  void redo() override;

private:
  LayerManager *m_layerManager;
  int m_layerIndex;
};

class SwitchOnionCommand : public QUndoCommand {
public:
  SwitchOnionCommand(LayerManager *layerManager, int layer,
                     QUndoCommand *parent = nullptr);
  ~SwitchOnionCommand() override;

  void undo() override;
  void redo() override;

private:
  LayerManager *m_layerManager;
  int m_layerIndex;
};

class SwitchHasMaskCommand : public QUndoCommand {
public:
  SwitchHasMaskCommand(LayerManager *layerManager, int layer,
                       QUndoCommand *parent = nullptr);
  ~SwitchHasMaskCommand() override;

  void undo() override;
  void redo() override;

private:
  LayerManager *m_layerManager;
  int m_layerIndex;
};


#endif // LAYERCOMMANDS_H
