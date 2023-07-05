/*
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __UVHASH_H__
#define __UVHASH_H__

#include <QtGlobal>
#include <QTransform>
#include <QHash>

#include "point.h"

struct UVInfo {
    int quadKey = INT_MAX;
    Point::VectorType uv; 
};

// Maps a unique point index to a UVInfo struct
// The unique point index is computed from its stroke id and position inside the stroke with the cantor pairing function
// TODO: bake the id in the point?
class UVHash : public QHash<unsigned int, UVInfo> {
public:
    inline virtual unsigned int cantor(unsigned int a, unsigned int b) const {
        return 0.5 * (a + b) * (a + b + 1) + b;
    }

    inline virtual bool has(unsigned int strokeIdx, unsigned int i) const {
        return contains(cantor(strokeIdx, i));
    }

    inline virtual QHash::iterator add(unsigned int strokeIdx, unsigned int i, const UVInfo &uv) {
        return insert(cantor(strokeIdx, i), uv);
    }

    inline virtual UVInfo get(unsigned int strokeIdx, unsigned int i) const {
        return value(cantor(strokeIdx, i));
    }

private:
};

#endif // __UVHASH_H__