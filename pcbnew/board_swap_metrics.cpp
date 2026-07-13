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

#include <board_swap_metrics.h>

#include <board.h>
#include <footprint.h>
#include <pad.h>

#include <limits>
#include <set>
#include <vector>


/// Total edge length of a Euclidean minimum spanning tree over the given points (Prim's algorithm).
static double euclideanMSTLength( const std::vector<VECTOR2I>& aPoints )
{
    const size_t n = aPoints.size();

    if( n < 2 )
        return 0.0;

    std::vector<bool>   inTree( n, false );
    std::vector<double> minDist( n, std::numeric_limits<double>::max() );
    minDist[0] = 0.0;

    double total = 0.0;

    for( size_t iter = 0; iter < n; ++iter )
    {
        int    pick = -1;
        double best = std::numeric_limits<double>::max();

        for( size_t i = 0; i < n; ++i )
        {
            if( !inTree[i] && minDist[i] < best )
            {
                best = minDist[i];
                pick = static_cast<int>( i );
            }
        }

        if( pick < 0 )
            break; // shouldn't happen for a connected point set

        inTree[pick] = true;
        total += minDist[pick];

        for( size_t v = 0; v < n; ++v )
        {
            if( !inTree[v] )
            {
                double d = ( aPoints[pick] - aPoints[v] ).EuclideanNorm();

                if( d < minDist[v] )
                    minDist[v] = d;
            }
        }
    }

    return total;
}


/**
 * Total MST length over @p aNets, where each pad's net is taken from @p aAssignment when present,
 * otherwise its current board net. This is the shared core: the delta estimate and the optimizer
 * both score a candidate assignment against another with it.
 */
static double totalNetMSTLength( const BOARD* aBoard, const std::set<int>& aNets,
                                 const std::map<const PAD*, int>& aAssignment )
{
    if( aNets.empty() )
        return 0.0;

    std::map<int, std::vector<VECTOR2I>> buckets;

    for( const FOOTPRINT* fp : aBoard->Footprints() )
    {
        for( const PAD* pad : fp->Pads() )
        {
            auto      it = aAssignment.find( pad );
            const int net = ( it != aAssignment.end() ) ? it->second : pad->GetNetCode();

            if( aNets.count( net ) )
                buckets[net].push_back( pad->GetPosition() );
        }
    }

    double total = 0.0;

    for( int net : aNets )
        total += euclideanMSTLength( buckets[net] );

    return total;
}


double EstimateSwapRatsnestDelta( const BOARD* aBoard, const std::map<const PAD*, int>& aNewNetByPad )
{
    if( !aBoard || aNewNetByPad.empty() )
        return 0.0;

    // Only the nets a pad leaves or joins can change length.
    std::set<int> affected;

    for( const auto& [pad, newNet] : aNewNetByPad )
    {
        affected.insert( pad->GetNetCode() );
        affected.insert( newNet );
    }

    affected.erase( 0 ); // the unconnected "net" has no ratsnest

    if( affected.empty() )
        return 0.0;

    const std::map<const PAD*, int> current; // empty = current board nets
    return totalNetMSTLength( aBoard, affected, aNewNetByPad )
           - totalNetMSTLength( aBoard, affected, current );
}


std::vector<GATE_SWAP_PLAN_ITEM> PlanGateSwapOptimization( BOARD* aBoard )
{
    std::vector<GATE_SWAP_PLAN_ITEM> plan;

    if( !aBoard )
        return plan;

    constexpr double IMPROVEMENT_EPSILON = 1.0; // nm; ignore sub-nm floating-point noise

    for( FOOTPRINT* fp : aBoard->Footprints() )
    {
        const std::vector<FOOTPRINT::FP_UNIT_INFO>& units = fp->GetUnitInfo();

        if( units.size() < 2 || !fp->AreUnitsInterchangeable() )
            continue;

        // Working net assignment for this footprint's gate pads; composes swaps across iterations.
        std::map<const PAD*, int> assignment;

        auto padNet = [&]( const PAD* aPad ) -> int
        {
            auto it = assignment.find( aPad );
            return ( it != assignment.end() ) ? it->second : aPad->GetNetCode();
        };

        // Greedily apply the single best-improving pairwise gate swap until none shortens routing.
        // Termination is guaranteed independently of the cap: every accepted swap strictly reduces
        // the total (bounded, non-negative) MST length by more than IMPROVEMENT_EPSILON, and no
        // permutation state repeats. The cap is only a safety valve against floating-point pathology;
        // it is generous so it never truncates a legitimate greedy chain for a real part.
        const size_t maxIters = units.size() * units.size() * units.size() + 8;

        for( size_t guard = 0; guard < maxIters; ++guard )
        {
            double                    bestDelta = -IMPROVEMENT_EPSILON;
            size_t                    bestI = 0;
            size_t                    bestJ = 0;
            std::map<const PAD*, int> bestProposal;

            for( size_t i = 0; i < units.size(); ++i )
            {
                for( size_t j = i + 1; j < units.size(); ++j )
                {
                    // Only equal-pin-count gates are interchangeable.
                    if( units[i].m_pins.size() != units[j].m_pins.size() )
                        continue;

                    std::map<const PAD*, int> proposal = assignment;
                    std::set<int>             affected;
                    bool                      ok = true;

                    for( size_t p = 0; p < units[i].m_pins.size(); ++p )
                    {
                        PAD* padI = fp->FindPadByNumber( units[i].m_pins[p] );
                        PAD* padJ = fp->FindPadByNumber( units[j].m_pins[p] );

                        if( !padI || !padJ )
                        {
                            ok = false;
                            break;
                        }

                        int netI = padNet( padI );
                        int netJ = padNet( padJ );

                        proposal[padI] = netJ;
                        proposal[padJ] = netI;

                        if( netI )
                            affected.insert( netI );

                        if( netJ )
                            affected.insert( netJ );
                    }

                    if( !ok || affected.empty() )
                        continue;

                    double delta = totalNetMSTLength( aBoard, affected, proposal )
                                   - totalNetMSTLength( aBoard, affected, assignment );

                    if( delta < bestDelta )
                    {
                        bestDelta = delta;
                        bestI = i;
                        bestJ = j;
                        bestProposal = std::move( proposal );
                    }
                }
            }

            if( bestDelta >= -IMPROVEMENT_EPSILON )
                break; // no improving swap left

            assignment = std::move( bestProposal );
            plan.push_back( { fp, units[bestI].m_unitName, units[bestJ].m_unitName, bestDelta } );
        }
    }

    return plan;
}
