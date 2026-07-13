/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef GATE_SWAP_H
#define GATE_SWAP_H

#include <utility>
#include <vector>

class BOARD;
class BOARD_CONNECTED_ITEM;
class FOOTPRINT;
class PAD;
struct GATE_SWAP_PLAN_ITEM;

/**
 * The net reassignments a single gate swap entails, split so a caller can treat the always-applied
 * changes (the gate pads themselves plus their connected tracks/vias) separately from the optional
 * ones (non-selected pads sharing copper), which an interactive caller may prompt about.
 */
struct GATE_SWAP_NET_CHANGES
{
    std::vector<std::pair<BOARD_CONNECTED_ITEM*, int>> m_always;   ///< gate pads + tracks/vias
    std::vector<std::pair<PAD*, int>>                  m_optional; ///< non-selected connected pads
};

enum class GATE_SWAP_RESULT
{
    OK,
    UNEQUAL_PINS, ///< the units do not all have the same pin count
    MISSING_PAD,  ///< a unit pin has no matching pad on the footprint
    NO_CHANGE     ///< the units already carry identical nets at every position (a no-op)
};

/**
 * Compute (without applying) the net reassignments for rotating nets forward among @p aUnitIndices
 * of @p aFootprint (unit i receives unit i+1's nets). Frame-independent: shared by the interactive
 * gate swap and the headless optimizer/CLI applier. Reads board connectivity to propagate the swap
 * onto connected copper. Returns GATE_SWAP_RESULT::OK and fills @p aChanges on success.
 */
GATE_SWAP_RESULT ComputeGateSwapNetChanges( BOARD* aBoard, FOOTPRINT* aFootprint,
                                            const std::vector<int>& aUnitIndices,
                                            GATE_SWAP_NET_CHANGES&  aChanges );

/**
 * Apply a gate-swap optimization plan directly to @p aBoard (no undo/commit, no UI), including the
 * connected-copper propagation for each swap. For headless use (kicad-cli, batch). @p aBoard's
 * connectivity must be current. Returns the number of plan steps actually applied.
 */
int ApplyGateSwapPlan( BOARD* aBoard, const std::vector<GATE_SWAP_PLAN_ITEM>& aPlan );

#endif // GATE_SWAP_H
