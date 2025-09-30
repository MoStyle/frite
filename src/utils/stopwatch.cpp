/*
 * SPDX-FileCopyrightText: 2021 Gael Guennebaud
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include "stopwatch.h"

int g_stopwatch_level = 0;
std::stack<double> g_stopwatch_stack;
