/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2018-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "layermanager.h"
#include "layer.h"
#include "editor.h"
#include "layercommands.h"
#include "keycommands.h"

LayerManager::LayerManager(QObject* pParent) : BaseManager(pParent) { m_currentLayerIndex = -1; }

LayerManager::~LayerManager() { 
    clear(); 
}

void LayerManager::clear() {
    m_layers.clear();
    m_indices.clear();
    m_currentLayerIndex = -1;
}

bool LayerManager::load(QDomElement& element, const QString& path) {
    if (element.isNull()) return false;

    clear();

    bool success = true;
    for (QDomNode node = element.firstChild(); !node.isNull(); node = node.nextSibling()) {
        QDomElement element = node.toElement();
        if (element.tagName() == "layer") {
            Layer* layer = newLayer();
            success = success & layer->load(element, path);
        }
    }

    // Update the map and indices list with the new layer idx
    QMap<int, std::shared_ptr<Layer>> newLayers;
    QList<int> newIndices(m_indices.size());
    for (auto it = m_layers.constBegin(); it != m_layers.constEnd(); ++it) {
        int newId = it.value()->id();
        int oldId = it.key();
        newLayers.insert(newId, it.value());
        int idx = m_indices.indexOf(oldId);
        newIndices[idx] = newId;
    }
    m_layers.swap(newLayers);
    m_indices.swap(newIndices);
    return success;
}

bool LayerManager::save(QDomDocument& doc, QDomElement& root, const QString& path) const {
    for (int i = 0; i < layersCount(); ++i) {
        m_layers[m_indices[i]]->save(doc, root, path);
    }
    return true;
}

Layer* LayerManager::currentLayer() const { 
    return currentLayer(0); 
}

Layer* LayerManager::currentLayer(int incr) const { 
    return layerAt(m_currentLayerIndex + incr); 
}

std::shared_ptr<Layer> LayerManager::currentLayerSharedPtr() const {
    return layerAtSharedPtr(m_currentLayerIndex);
}

/**
 * @param index the layer order! (not its internal m_id!)
*/
Layer* LayerManager::layerAt(int index) const {
    if (index < 0 || index >= layersCount()) {
        return nullptr;
    }

    return m_layers[m_indices[index]].get();
}

/**
 * @param index the layer order! (not its internal m_id!)
*/
std::shared_ptr<Layer> LayerManager::layerAtSharedPtr(int index) const {
    if (index < 0 || index >= layersCount()) {
        return nullptr;
    }

    return m_layers[m_indices[index]];
}

void LayerManager::moveLayer(int i, int j) {
    if (i < 0 || i >= m_layers.size() || j < 0 || j >= m_layers.size() || i == j) return;

    std::swap(m_indices[i], m_indices[j]);
}

int LayerManager::currentLayerIndex() { 
    return m_currentLayerIndex; 
}

void LayerManager::setCurrentLayer(int layerIndex) {
    if (layerIndex >= layersCount()) {
        Q_ASSERT(false);
        return;
    }

    if (m_currentLayerIndex != layerIndex) {
        m_currentLayerIndex = layerIndex;
        emit currentLayerChanged(m_currentLayerIndex);
    }
}

void LayerManager::setCurrentLayer(Layer* layer) {
    for (int i = 0; i < layersCount(); ++i) {
        if (layer == layerAt(i)) {
            setCurrentLayer(i);
            return;
        }
    }
}

Layer* LayerManager::newLayer() {
    Layer* layer = createLayer(m_layers.size());
    layer->addNewEmptyKeyAt(1);
    return layer;
}

void LayerManager::addLayer() { m_editor->undoStack()->push(new AddLayerCommand(this, m_currentLayerIndex + 1)); }

Layer* LayerManager::createLayer(int layerIndex) {
    std::shared_ptr<Layer> layer = std::make_shared<Layer>(m_editor);
    m_indices.insert(layerIndex, layer->id());
    m_layers.insert(layer->id(), layer);
    layer->setName(QString("Layer %1").arg(layer->id()));
    m_currentLayerIndex = layerIndex;

    emit layerCountChanged(layersCount());
    return layer.get();
}

int LayerManager::lastFrameAtFrame(int frameIndex) {
    for (int i = frameIndex; i >= 0; i -= 1) {
        for (int layerIndex = 0; layerIndex < layersCount(); ++layerIndex) {
            auto layer = layerAt(layerIndex);
            if (layer->keyExists(i)) {
                return i;
            }
        }
    }
    return -1;
}

int LayerManager::firstKeyFrameIndex() {
    int minPosition = INT_MAX;

    for (int i = 0; i < layersCount(); ++i) {
        Layer* layer = layerAt(i);

        int position = layer->firstKeyFramePosition();
        if (position < minPosition) {
            minPosition = position;
        }
    }
    return minPosition;
}

int LayerManager::lastKeyFrameIndex() {
    int maxPosition = 0;

    for (int i = 0; i < layersCount(); ++i) {
        Layer* layer = layerAt(i);

        int position = layer->getMaxKeyFramePosition();
        if (position > maxPosition) {
            maxPosition = position;
        }
    }
    return maxPosition;
}

void LayerManager::deleteCurrentLayer() {
    m_editor->undoStack()->beginMacro("Delete layer");
    if (m_currentLayerIndex > -1 && m_currentLayerIndex < m_layers.size()) {
        Layer* layer = m_layers[m_indices[m_currentLayerIndex]].get();
        QList<int> keys = layer->keys();
        keys.takeLast();
        for (int k : keys) m_editor->undoStack()->push(new RemoveKeyCommand(m_editor, m_currentLayerIndex, k));
    }
    m_editor->undoStack()->push(new RemoveLayerCommand(this, m_currentLayerIndex));
    m_editor->undoStack()->endMacro();
}

void LayerManager::deleteLayer(int layerIndex) {
    if (layerIndex > -1 && layerIndex < m_layers.size()) {
        // delete m_layers[m_indices[layerIndex]];
        m_layers.remove(m_indices[layerIndex]);
        m_indices.removeAt(layerIndex);
    }

    if (m_layers.isEmpty()) m_editor->undoStack()->push(new AddLayerCommand(this, layerIndex));

    if (currentLayerIndex() == layersCount()) setCurrentLayer(currentLayerIndex() - 1);

    emit layerCountChanged(layersCount());
}

void LayerManager::destroyBuffers() {
    for (auto layer : m_layers) {
        for (auto it = layer->keysBegin(); it != layer->keysEnd(); ++it) {
            it.value()->destroyBuffers();
        }
    }
}

void LayerManager::gotoNextLayer() {
    if (m_currentLayerIndex < layersCount() - 1) {
        m_currentLayerIndex += 1;
        emit currentLayerChanged(m_currentLayerIndex);
    }
}

void LayerManager::gotoPreviouslayer() {
    if (m_currentLayerIndex > 0) {
        m_currentLayerIndex -= 1;
        emit currentLayerChanged(m_currentLayerIndex);
    }
}

int LayerManager::maxFrame() {
    int maxFrame = -1;

    for (int i = 0; i < layersCount(); i++) {
        int frame = layerAt(i)->getMaxKeyFramePosition();
        if (frame > maxFrame) maxFrame = frame;
    }
    return maxFrame;
}

void LayerManager::layerUpdated(int layerId) { emit currentLayerChanged(layerId); }
