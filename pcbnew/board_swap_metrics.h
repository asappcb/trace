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

#ifndef BOARD_SWAP_METRICS_H
#define BOARD_SWAP_METRICS_H

#include <map>
#include <vector>

#include <wx/string.h>

class BOARD;
class FOOTPRINT;
class PAD;

/**
 * Estimate how a hypothetical net reassignment (a gate or pin swap) would change total routing
 * length, using the minimum-spanning-tree length of the affected nets' pads as a geometric proxy
 * for the ratsnest. This is a preview/scoring estimate: it uses straight-line pad-to-pad distance
 * and does not account for layers, obstacles, or the router, so a proposed swap must still be
 * validated for legality/routability before it is trusted. It also scores only the pads named in
 * @p aNewNetByPad; the connected-item net propagation that an actual swap performs on a routed board
 * (tracks/vias/other pads sharing copper) is not modelled, so on a routed board the estimate can
 * diverge from the post-swap ratsnest.
 *
 * @param aBoard        the board whose pads are measured.
 * @param aNewNetByPad  pads that would receive a new net code under the swap (pad -> new net code).
 *                      Pads not in the map keep their current net.
 * @return (total MST length after) - (before) over the affected nets, in internal units. Negative
 *         means the swap shortens the ratsnest (a routing improvement); zero means no change.
 */
double EstimateSwapRatsnestDelta( const BOARD* aBoard, const std::map<const PAD*, int>& aNewNetByPad );


/**
 * One planned gate swap: exchange the nets of gates @ref m_unitA and @ref m_unitB (by
 * FOOTPRINT::FP_UNIT_INFO unit name) on @ref m_footprint. @ref m_delta is the estimated ratsnest
 * length change of this step given the swaps planned before it (negative = shorter).
 */
struct GATE_SWAP_PLAN_ITEM
{
    FOOTPRINT* m_footprint = nullptr;
    wxString   m_unitA;
    wxString   m_unitB;
    double     m_delta = 0.0;
};

/**
 * Plan a sequence of gate swaps that shorten total routing. For each footprint whose units are
 * interchangeable, greedily swaps equal-pin-count gate pairs while any swap shortens the ratsnest
 * (using the same MST metric as EstimateSwapRatsnestDelta), composing swaps within the footprint.
 *
 * Pure: it reads the board and mutates nothing; the caller applies the returned ordered list (e.g.
 * through the existing gate-swap machinery) and validates routability. The greedy pairwise search
 * finds a local optimum, not necessarily the global one.
 */
std::vector<GATE_SWAP_PLAN_ITEM> PlanGateSwapOptimization( BOARD* aBoard );

#endif // BOARD_SWAP_METRICS_H
