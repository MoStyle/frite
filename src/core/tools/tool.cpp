/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "tool.h"
#include "editor.h"

void Tool::toggled(bool on) {
    m_editor->updateStatusBar(m_toolTips, 0);
}