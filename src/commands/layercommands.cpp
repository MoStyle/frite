/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "layercommands.h"
#include "layermanager.h"
#include "layer.h"

AddLayerCommand::AddLayerCommand(LayerManager* layerManager, int layer, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_layerManager(layerManager)
    , m_layerName()
    , m_layerIndex(layer)
{
    setText("Add layer");
}

AddLayerCommand::~AddLayerCommand()
{
}

void AddLayerCommand::undo()
{
    m_layerManager->deleteLayer(m_layerIndex);
}

void AddLayerCommand::redo()
{
    Layer* newLayer = m_layerManager->createLayer(m_layerIndex);
    newLayer->addNewEmptyKeyAt(1);
    if(!m_layerName.isEmpty())
        newLayer->setName(m_layerName);
    m_layerName = newLayer->name();
}

RemoveLayerCommand::RemoveLayerCommand(LayerManager* layerManager, int layerIndex, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_layerManager(layerManager)
    , m_layerName()
    , m_layerIndex(layerIndex)
{
    setText("Remove layer");
}

RemoveLayerCommand::~RemoveLayerCommand()
{
}

void RemoveLayerCommand::undo()
{
    m_layerManager->createLayer(m_layerIndex)->setName(m_layerName);
}

void RemoveLayerCommand::redo()
{
    m_layerName = m_layerManager->layerAt(m_layerIndex)->name();
    m_layerManager->deleteLayer(m_layerIndex);
}

MoveLayerCommand::MoveLayerCommand(LayerManager* layerManager, int layerIndex1, int layerIndex2, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_layerManager(layerManager)
    , m_layerIndex1(layerIndex1)
    , m_layerIndex2(layerIndex2)
{
    setText("Move layer");
}

MoveLayerCommand::~MoveLayerCommand()
{
}

void MoveLayerCommand::undo()
{
    m_layerManager->moveLayer(m_layerIndex2, m_layerIndex1);
}

void MoveLayerCommand::redo()
{
    m_layerManager->moveLayer(m_layerIndex1, m_layerIndex2);
}

ChangeOpacityCommand::ChangeOpacityCommand(LayerManager* layerManager, int layer, qreal opacity, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_layerManager(layerManager)
    , m_layerIndex(layer)
    , m_opacity(opacity)
{
    setText("Change layer opacity");
}

ChangeOpacityCommand::~ChangeOpacityCommand()
{
}

void ChangeOpacityCommand::undo()
{
    m_layerManager->layerAt(m_layerIndex)->setOpacity(m_prevOpacity);
}

void ChangeOpacityCommand::redo()
{
    Layer* layer = m_layerManager->layerAt(m_layerIndex);
    m_prevOpacity = layer->opacity();
    layer->setOpacity(m_opacity);
}

SwitchVisibilityCommand::SwitchVisibilityCommand(LayerManager *layerManager, int layer, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_layerManager(layerManager)
    , m_layerIndex(layer)
{
    setText("Switch layer visibility");
}

SwitchVisibilityCommand::~SwitchVisibilityCommand()
{
}

void SwitchVisibilityCommand::undo()
{
    m_layerManager->layerAt(m_layerIndex)->switchVisibility();
}

void SwitchVisibilityCommand::redo()
{
    m_layerManager->layerAt(m_layerIndex)->switchVisibility();
}

SwitchOnionCommand::SwitchOnionCommand(LayerManager *layerManager, int layer, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_layerManager(layerManager)
    , m_layerIndex(layer)
{
    setText("Switch onion skin");
}

SwitchOnionCommand::~SwitchOnionCommand()
{
}

void SwitchOnionCommand::undo()
{
    m_layerManager->layerAt(m_layerIndex)->switchShowOnion();
}

void SwitchOnionCommand::redo()
{
    m_layerManager->layerAt(m_layerIndex)->switchShowOnion();
}
