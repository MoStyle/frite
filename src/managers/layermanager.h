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
    Layer* currentLayer() const;
    Layer* currentLayer(int offset) const;
    std::shared_ptr<Layer> currentLayerSharedPtr() const;
    Layer* layerAt(int index) const;
    std::shared_ptr<Layer> layerAtSharedPtr(int index) const;
    // inline QList<std::shared_ptr<Layer>> layers() const { return m_layers.values(); }
    const QList<int> &indices() const { return m_indices; }
    int currentLayerIndex();
    void setCurrentLayer(Layer* layer);

    // KeyFrame Management
    int lastFrameAtFrame(int frameIndex);
    int firstKeyFrameIndex();
    int lastKeyFrameIndex();

    int maxFrame();

    void layerUpdated(int layerId);
    void moveLayer(int i, int j);
    Layer* createLayer(int layerIndex);
    void deleteLayer(int layerIndex);
    void destroyBuffers();

   public slots:
    void setCurrentLayer(int nIndex);
    Layer* newLayer();
    void addLayer();
    void deleteCurrentLayer();
    void gotoNextLayer();
    void gotoPreviouslayer();

   signals:
    void currentLayerChanged(int n);
    void layerCountChanged(int layersCount);

   private:
    QList<int> m_indices;                           // layers ordering (list of layer's m_id)
    QMap<int, std::shared_ptr<Layer>> m_layers;     // layers are hashed by their m_id

    int m_currentLayerIndex;  // the current layer to be edited/displayed
};

#endif
