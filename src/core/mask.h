#ifndef __MASK_H__
#define __MASK_H__

#include <QTransform>
#include "point.h"
#include "corner.h"

#include <vector>
#include <clipper2/clipper.h>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLShaderProgram>

class TESStesselator;
class Group;
class VectorKeyFrame;
class Corner;

class Mask {
public:
    Mask(Group *group, bool forwardMask);
    ~Mask();

    void computeOutline();
    bool isDirty() const { return m_dirty; }
    void setDirty() { m_dirty = true; }

    const Clipper2Lib::PathD &polygon() const { return m_polygon; }
    TESStesselator *tessellator() const { return m_tessellator; }

    // Must be called with a valid OpenGL context
    void bindVAO() { m_vao.bind(); }
    void releaseVAO() { m_vao.release(); }
    void createBuffer(QOpenGLShaderProgram *program, VectorKeyFrame *keyframe, int inbetween);
    void destroyBuffer();
    void updateBuffer(VectorKeyFrame *keyframe, int inbetween);

    struct OutlineVertexInfo {
        int cornerKey = INT_MAX;
        int quadKey = INT_MAX;
        Point::VectorType uv;
        bool antialias;
    };
    const std::vector<OutlineVertexInfo> &vertexInfo() const { return m_outlineVertexInfo; }

private:
    void projectOutline();
    void smoothOutline();
    void projectToGrid();
    void tessellate();
    void computeUVs();

    Group *m_group;
    std::unique_ptr<Lattice> m_grid;
    Clipper2Lib::PathD m_polygon;
    std::vector<OutlineVertexInfo> m_outlineVertexInfo;
    TESStesselator *m_tessellator;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo, m_ebo;
    bool m_bufferCreated, m_bufferDestroyed, m_bufferDirty;
    bool m_forwardMask, m_dirty;
};

#endif // __MASK_H__