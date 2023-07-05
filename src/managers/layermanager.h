/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2018-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LAYER_MANAGER_H
#define LAYER_MANAGER_H

#include "basemanager.h"
#include <QList>
#include <QDomElement>

class Layer;
class Editor;

class LayerManager : public BaseManager {
    Q_OBJECT
   public:
    LayerManager(QObject* pParant);
    ~LayerManager();

    void clear();

    bool load(QDomElement& element, const QString& path);
    bool save(QDomDocument& doc, QDomElement& root, const QString& path) const;

    // Layer Management
    inline int layersCount() const { return m_layers.size(); }
    Layer* currentLayer();
    Layer* currentLayer(int offset);
    Layer* layerAt(int index);
    inline QList<Layer*> layers() const { return m_layers.values(); }
    const QList<int> &indices() const { return m_indices; }
    const QMap<int, Layer*> &layersMap() const { return m_layers; }
    int currentLayerIndex();
    void setCurrentLayer(Layer* layer);

    void gotoNextLayer();
    void gotoPreviouslayer();

    // KeyFrame Management
    int lastFrameAtFrame(int frameIndex);
    int firstKeyFrameIndex();
    int lastKeyFrameIndex();

    int maxFrame();

    void layerUpdated(int layerId);
    void moveLayer(int i, int j);
    Layer* createLayer(int layerIndex);
    void deleteLayer(int layerIndex);

   public slots:
    void setCurrentLayer(int nIndex);
    Layer* newLayer();
    void addLayer();
    void deleteCurrentLayer();

   signals:
    void currentLayerChanged(int n);
    void layerCountChanged(int layersCount);

   private:
    QList<int> m_indices;           // maps a layer idx order to a layer's m_id 
    QMap<int, Layer*> m_layers;     // layers are hashed by their m_id

    int m_currentLayerIndex;  // the current layer to be edited/displayed
};

#endif
