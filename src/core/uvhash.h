/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __UVHASH_H__
#define __UVHASH_H__

#include <QtGlobal>
#include <QTransform>
#include <QHash>

#include "point.h"
#include "utils/utils.h"

struct UVInfo {
    int quadKey = INT_MAX;
    Point::VectorType uv;
};

// Maps a unique point index to a UVInfo struct
// The unique point index is computed from its stroke id and position inside the stroke with the cantor pairing function
// TODO: bake the id in the point?
class UVHash : public QHash<unsigned int, UVInfo> {
public:
    inline virtual bool has(unsigned int strokeIdx, unsigned int i) const {
        return contains(Utils::cantor(strokeIdx, i));
    }

    inline virtual QHash::iterator add(unsigned int strokeIdx, unsigned int i, const UVInfo &uv) {
        return insert(Utils::cantor(strokeIdx, i), uv);
    }

    inline virtual UVInfo get(unsigned int strokeIdx, unsigned int i) const {
        return value(Utils::cantor(strokeIdx, i));
    }
private:
};

#endif // __UVHASH_H__