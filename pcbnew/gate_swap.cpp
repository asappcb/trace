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

#include <gate_swap.h>
#include <board_swap_metrics.h>

#include <board.h>
#include <footprint.h>
#include <pad.h>
#include <pcb_track.h>
#include <connectivity/connectivity_data.h>

#include <unordered_map>
#include <unordered_set>


GATE_SWAP_RESULT ComputeGateSwapNetChanges( BOARD* aBoard, FOOTPRINT* aFootprint,
                                            const std::vector<int>& aUnitIndices,
                                            GATE_SWAP_NET_CHANGES&  aChanges )
{
    aChanges = GATE_SWAP_NET_CHANGES();

    const std::vector<FOOTPRINT::FP_UNIT_INFO>& units = aFootprint->GetUnitInfo();
    const size_t                                unitCount = aUnitIndices.size();

    if( unitCount < 2 )
        return GATE_SWAP_RESULT::NO_CHANGE;

    const size_t pinCount = units[aUnitIndices.front()].m_pins.size();

    for( int idx : aUnitIndices )
    {
        if( units[idx].m_pins.size() != pinCount )
            return GATE_SWAP_RESULT::UNEQUAL_PINS;
    }

    // Build per-unit pad arrays and net vectors.
    std::vector<std::vector<PAD*>> unitPads( unitCount );
    std::vector<std::vector<int>>  unitNets( unitCount );

    for( size_t ui = 0; ui < unitCount; ++ui )
    {
        const std::vector<wxString>& pins = units[aUnitIndices[ui]].m_pins;

        for( size_t pi = 0; pi < pinCount; ++pi )
        {
            PAD* p = aFootprint->FindPadByNumber( pins[pi] );

            if( !p )
                return GATE_SWAP_RESULT::MISSING_PAD;

            unitPads[ui].push_back( p );
            unitNets[ui].push_back( p->GetNetCode() );
        }
    }

    // If all unit nets match across positions, nothing to do.
    bool allSame = true;

    for( size_t pi = 0; pi < pinCount && allSame; ++pi )
    {
        int refNet = unitNets[0][pi];

        for( size_t ui = 1; ui < unitCount; ++ui )
        {
            if( unitNets[ui][pi] != refNet )
            {
                allSame = false;
                break;
            }
        }
    }

    if( allSame )
        return GATE_SWAP_RESULT::NO_CHANGE;

    std::shared_ptr<CONNECTIVITY_DATA> connectivity = aBoard->GetConnectivity();

    std::unordered_map<BOARD_CONNECTED_ITEM*, int> itemNewNets;
    std::vector<PAD*>                              nonSelectedPadsToChange;
    std::unordered_set<PAD*>                       swapPads;

    for( const auto& v : unitPads )
        swapPads.insert( v.begin(), v.end() );

    auto scheduleForPad = [&]( PAD* pad, int fromNet, int toNet )
    {
        for( BOARD_CONNECTED_ITEM* ci : connectivity->GetConnectedItems( pad, 0 ) )
        {
            switch( ci->Type() )
            {
            case PCB_TRACE_T:
            case PCB_ARC_T:
            case PCB_VIA_T:
            case PCB_PAD_T:
                break;

            default:
                continue;
            }

            if( ci->GetNetCode() != fromNet )
                continue;

            itemNewNets[ci] = toNet;

            if( ci->Type() == PCB_PAD_T )
            {
                PAD* other = static_cast<PAD*>( ci );

                if( !swapPads.count( other ) )
                    nonSelectedPadsToChange.push_back( other );
            }
        }
    };

    // For each position, rotate nets among units forward.
    for( size_t pi = 0; pi < pinCount; ++pi )
    {
        for( size_t ui = 0; ui < unitCount; ++ui )
        {
            size_t toIdx = ( ui + 1 ) % unitCount;
            scheduleForPad( unitPads[ui][pi], unitNets[ui][pi], unitNets[toIdx][pi] );
        }
    }

    // The gate pads themselves always change to their rotated net.
    for( size_t pi = 0; pi < pinCount; ++pi )
    {
        for( size_t ui = 0; ui < unitCount; ++ui )
        {
            size_t toIdx = ( ui + 1 ) % unitCount;
            aChanges.m_always.emplace_back( unitPads[ui][pi], unitNets[toIdx][pi] );
        }
    }

    // Tracks/vias always follow; non-selected connected pads are optional.
    std::unordered_set<PAD*> optionalPadSet( nonSelectedPadsToChange.begin(),
                                             nonSelectedPadsToChange.end() );

    for( const auto& [item, newNet] : itemNewNets )
    {
        if( item->Type() == PCB_PAD_T )
        {
            PAD* p = static_cast<PAD*>( item );

            if( swapPads.count( p ) )
                continue; // already covered by the gate rotation above

            if( optionalPadSet.count( p ) )
                aChanges.m_optional.emplace_back( p, newNet );
        }
        else
        {
            aChanges.m_always.emplace_back( item, newNet );
        }
    }

    return GATE_SWAP_RESULT::OK;
}


int ApplyGateSwapPlan( BOARD* aBoard, const std::vector<GATE_SWAP_PLAN_ITEM>& aPlan )
{
    if( !aBoard )
        return 0;

    int applied = 0;

    for( const GATE_SWAP_PLAN_ITEM& step : aPlan )
    {
        GATE_SWAP_NET_CHANGES changes;

        if( ComputeGateSwapNetChanges( aBoard, step.m_footprint,
                                       { step.m_unitIndexA, step.m_unitIndexB }, changes )
            != GATE_SWAP_RESULT::OK )
        {
            continue;
        }

        for( const auto& [item, net] : changes.m_always )
            item->SetNetCode( net );

        // Headless application includes connected pads (no prompt).
        for( const auto& [pad, net] : changes.m_optional )
            pad->SetNetCode( net );

        ++applied;
    }

    return applied;
}
