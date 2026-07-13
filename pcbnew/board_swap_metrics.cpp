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

    // Bucket every board pad's position by its net, both under the current assignment and under the
    // hypothetical one, in a single pass.
    std::map<int, std::vector<VECTOR2I>> before;
    std::map<int, std::vector<VECTOR2I>> after;

    for( const FOOTPRINT* fp : aBoard->Footprints() )
    {
        for( const PAD* pad : fp->Pads() )
        {
            const int oldNet = pad->GetNetCode();

            auto      it = aNewNetByPad.find( pad );
            const int newNet = ( it != aNewNetByPad.end() ) ? it->second : oldNet;

            if( affected.count( oldNet ) )
                before[oldNet].push_back( pad->GetPosition() );

            if( affected.count( newNet ) )
                after[newNet].push_back( pad->GetPosition() );
        }
    }

    double delta = 0.0;

    for( int net : affected )
        delta += euclideanMSTLength( after[net] ) - euclideanMSTLength( before[net] );

    return delta;
}
