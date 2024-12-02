#include "tool.h"
#include "editor.h"

void Tool::toggled(bool on) {
    m_editor->updateStatusBar(m_toolTips, 0);
}