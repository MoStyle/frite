#ifndef BASEMANAGER_H
#define BASEMANAGER_H

#include "editor.h"

class BaseManager : public QObject
{
    Q_OBJECT
public:
    explicit BaseManager(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~BaseManager() { m_editor = nullptr; }

    void setEditor(Editor* editor)
    {
        Q_ASSERT_X( editor != nullptr, "BaseManager", "Editor is null." );
        m_editor = editor;
    }

    Editor* editor() { return m_editor; }

protected:
    Editor* m_editor = nullptr;
};

#endif // BASEMANAGER_H
