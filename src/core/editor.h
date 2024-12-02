/*

Pencil - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2013-2014 Matt Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation;

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef EDITOR_H
#define EDITOR_H

#include "vectorkeyframe.h"

#include "stroke.h"
#include "strokeinterval.h"

#include <memory>
#include <set>

#include <QDomElement>
#include <QtWidgets>

class QDragEnterEvent;
class QDropEvent;
class GridManager;
class RegistrationManager;
class ColorManager;
class LayerManager;
class PlaybackManager;
class ViewManager;
class InterpolationManager;
class ToolsManager;
class FixedSceneManager;
class SelectionManager;
class TimeLine;
class TabletCanvas;
class BackupElement;
class StyleManager;
class LayoutManager;
class VisibilityManager;
class KeyFrame;
class Stroke;
class Layer;
class QUndoStack;

struct EqualizerValues {
    int maxDistance;
    QMap<int, int> value;
    QMap<int, bool> state;
};

enum EqualizedMode { KEYS, FRAMES };

class Editor : public QObject {
    Q_OBJECT

   public:
    explicit Editor(QObject *parent = nullptr);
    virtual ~Editor();

    bool init(TabletCanvas *canvas);

    /************************************************************************/
    /* Managers                                                             */
    /************************************************************************/
    ColorManager *color() const { return m_colorManager; }
    LayerManager *layers() const { return m_layerManager; }
    PlaybackManager *playback() const { return m_playbackManager; }
    ViewManager *view() const { return m_viewManager; }
    StyleManager *style() const { return m_styleManager; }
    GridManager *grid() const { return m_gridManager; }
    RegistrationManager *registration() const { return m_registrationManager; }
    ToolsManager *tools() const { return m_toolsManager; }
    FixedSceneManager *fixedScene() const { return m_fixedSceneManager; }
    SelectionManager *selection() const { return m_selectionManager; }
    LayoutManager *layout() const { return m_layoutManager; }
    VisibilityManager *visibility() const { return m_visibilityManager; }

    void setTabletCanvas(TabletCanvas *canvas);
    TabletCanvas *tabletCanvas() { return m_tabletCanvas; }

    qreal alpha(int frame, Layer *layer=nullptr);
    qreal currentAlpha();
    void scrubTo(int frameNumber);

    void addStroke(StrokePtr stroke);
    void addEndStroke(StrokePtr stroke);

    bool load(QDomElement &element, const QString &path);
    bool save(QDomDocument &doc, QDomElement &root, const QString &path) const;

    int addKeyFrame(int layerNumber, int frameNumber, bool updateCurves=true);
    void removeKeyFrame(int layerNumber, int frameIndex);
    int updateInbetweens(VectorKeyFrame *keyframe, int inbetween, int stride);
    void deleteAllEmptyGroups(int layerNumber, int frameIndex);

    QUndoStack *undoStack() const { return m_undoStack; }

    void exportFrames(const QString &path, QSize exportSize, bool transparency = true);

    QColor backwardColor() const { return m_backwardColor; }
    void setBackwardColor(const QColor &backwardColor);
    QColor forwardColor() const { return m_forwardColor; }
    void setForwardColor(const QColor &forwardColor);
    void setEqValues(const EqualizerValues &value);
    EqualizedMode getEqMode() const { return m_eqMode; }
    int tintFactor() const { return m_tintFactor; }
    EqualizerValues getEqValues() const { return m_eqValues; }
    bool ghostMode() const { return m_ghostMode; }

    VectorKeyFrame *currentKeyFrame();
    VectorKeyFrame *prevKeyFrame();

    void registerFromRestPosition(VectorKeyFrame * key, bool registerToNextKeyframe);

   signals:
    void updateTimeLine();
    void updateBackup();

    void selectAll();
    void changeThinLinesButton(bool);

    void currentFrameChanged(int n);
    void currentKeyFrameChanged();
    void timelineUpdate(int n);
    void alphaChanged(qreal alpha);

    void updateStatusBar(const QString &message, int timeout);

    // save
    void needSave();
    void fileLoaded();

   public slots:
    void clearCurrentFrame();

    void deselectAll();

    void scrubForward();
    void scrubBackward();

    void addKey();
    void duplicateKey();
    void removeKey();

    void setCurrentLayer(int layerNumber);

    void copy();
    void paste();
    void cut();

    void increaseCurrentKeyExposure();
    void decreaseCurrentKeyExposure();

    void setEqMode(int value);
    void setTintFactor(int value);
    void setGhostMode(bool ghostMode);

    // update qgraphicsscenes and the group qwidget
    void updateUI(VectorKeyFrame *key);

    //  ******* ACTIONS *******
    void deselectInAllLayers();
    void clearARAPWarp();
    void toggleOnionSkin();
    void toggleHasMask();
    void makeTrajectoryC1Continuous();
    void makeGroupFadeOut();
    std::set<int> splitGridIntoSingleConnectedComponent();
    void regularizeLattice();
    void registerFromRestPosition();
    void registerFromTargetPosition();
    void changeGridSize();
    void expandGrid();
    void clearGrid();
    void copyGroupToNextKeyFrame(bool makeBreakdown);
    void convertToBreakdown();
    void toggleCrossFade();
    void clearCrossFade();
    void deleteGroup();
    void deleteAllEmptyGroups();
    void makeInbetweensDirty();
    void toggleDrawSplat(bool drawSplat);
    void drawInNewGroup();
    void suggestLayoutChange();
    void propagateLayoutForward();
    void propagateLayoutBackward();
    void suggestVisibilityThresholds();
    void debugReport();

   private:
    ColorManager *m_colorManager = nullptr;
    TabletCanvas *m_tabletCanvas = nullptr;
    PlaybackManager *m_playbackManager = nullptr;
    LayerManager *m_layerManager = nullptr;
    ViewManager *m_viewManager = nullptr;
    StyleManager *m_styleManager = nullptr;
    GridManager *m_gridManager = nullptr;
    RegistrationManager *m_registrationManager = nullptr;
    ToolsManager *m_toolsManager = nullptr;
    FixedSceneManager *m_fixedSceneManager = nullptr;
    SelectionManager *m_selectionManager = nullptr;
    LayoutManager *m_layoutManager = nullptr;
    VisibilityManager *m_visibilityManager = nullptr;

    QUndoStack *m_undoStack;

    bool m_ghostMode = false;
    bool m_exporting = false;

    std::vector<StrokeIntervals> m_clipboardStrokes;    
    VectorKeyFrame *m_clipboardKeyframe; // TODO: shared ptr in case kf is destroyed between copy & paste

    // Onion skins parameters
    EqualizerValues m_eqValues;
    EqualizedMode m_eqMode;
    QColor m_backwardColor, m_forwardColor;
    int m_tintFactor;
};

#endif
