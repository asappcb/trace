/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
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

#include <limits>
#include <optional>

#include <board.h>
#include <board_connected_item.h>
#include <board_design_settings.h>
#include <board_stackup_manager/board_stackup.h>
#include <connectivity/connectivity_data.h>
#include <footprint.h>
#include <pad.h>
#include <pcb_track.h>
#include <padstack.h>
#include <layer_ids.h>
#include <lset.h>
#include <drc/drc_engine.h>
#include <drc/drc_item.h>
#include <drc/drc_rule.h>
#include <drc/drc_test_provider.h>

/*
    Backdrill test.

    Structural validity: every backdrill (secondary/tertiary drill on a via or pad padstack)
    must describe a manufacturable span - start at an outer copper layer, start and must-cut
    layers both enabled copper, the two different, and the backdrill diameter not smaller than
    the primary drill it clears.

    Residual stub length: the un-removed portion of the via barrel that remains between the
    backdrill must-cut layer and the nearest same-net copper connection the backdrill approaches
    from its entry surface. That leftover barrel is the signal-integrity stub backdrilling exists
    to remove; it must not exceed a user backdrill_stub_length constraint. The residual is
    measured against the via's actual connections (not the board's outer layers), so a correctly
    backdrilled via - whose must-cut layer sits right past its signal exit - reports ~0.

    Errors generated:
    - DRCE_BACKDRILL_INVALID_SPAN
    - DRCE_BACKDRILL_STUB_TOO_LONG
*/

class DRC_TEST_PROVIDER_BACKDRILL : public DRC_TEST_PROVIDER
{
public:
    DRC_TEST_PROVIDER_BACKDRILL()
    {}

    virtual ~DRC_TEST_PROVIDER_BACKDRILL() = default;

    virtual bool Run() override;

    virtual const wxString GetName() const override { return wxT( "backdrill" ); };

private:
    void checkBackdrills( BOARD_ITEM* aItem, const PADSTACK& aPadstack, int aPrimaryDrill,
                          const LSET& aCopper, const BOARD_STACKUP& aStackup,
                          CONNECTIVITY_DATA* aConnectivity, bool aCheckStub );
};


void DRC_TEST_PROVIDER_BACKDRILL::checkBackdrills( BOARD_ITEM* aItem, const PADSTACK& aPadstack,
                                                   int aPrimaryDrill, const LSET& aCopper,
                                                   const BOARD_STACKUP& aStackup,
                                                   CONNECTIVITY_DATA* aConnectivity, bool aCheckStub )
{
    auto reportInvalidSpan =
            [&]( const PADSTACK::DRILL_PROPS& aDrill ) -> bool
            {
                wxString msg;

                if( aDrill.start == UNDEFINED_LAYER || aDrill.end == UNDEFINED_LAYER )
                    msg = _( "backdrill span is missing a start or must-cut layer" );
                else if( !aCopper.Contains( aDrill.start ) || !aCopper.Contains( aDrill.end ) )
                    msg = _( "backdrill span references a layer that is not an enabled copper layer" );
                else if( aDrill.start != F_Cu && aDrill.start != B_Cu )
                    msg = _( "backdrill must start at an outer copper layer" );
                else if( aDrill.start == aDrill.end )
                    msg = _( "backdrill start and must-cut layers are the same" );
                else if( aDrill.size.x > 0 && aPrimaryDrill > 0 && aDrill.size.x < aPrimaryDrill )
                    msg = _( "backdrill diameter is smaller than the primary drill diameter" );
                else
                    return false; // structurally valid

                std::shared_ptr<DRC_ITEM> drcItem = DRC_ITEM::Create( DRCE_BACKDRILL_INVALID_SPAN );
                drcItem->SetErrorDetail( msg );
                drcItem->SetItems( aItem );

                PCB_LAYER_ID markerLayer = aDrill.start != UNDEFINED_LAYER ? aDrill.start : F_Cu;
                reportViolation( drcItem, aItem->GetPosition(), markerLayer );
                return true;
            };

    // Residual barrel (IU) between the must-cut layer and the nearest same-net copper connection
    // the backdrill approaches from its entry surface, or nullopt if the must-cut already clears
    // every connection on that side / the span is unusable. re-validates the span first (guards
    // GetLayerDistance, which asserts on non-copper layers).
    auto residualStub =
            [&]( const PADSTACK::DRILL_PROPS& aDrill ) -> std::optional<int>
            {
                if( ( aDrill.start != F_Cu && aDrill.start != B_Cu )
                    || !aCopper.Contains( aDrill.end ) )
                {
                    return std::nullopt;
                }

                BOARD_CONNECTED_ITEM* connItem = dynamic_cast<BOARD_CONNECTED_ITEM*>( aItem );

                if( !connItem || !aConnectivity )
                    return std::nullopt;

                // Use the routing that actually lands on this via/pad (tracks and arcs at its
                // anchor), not the whole net cluster - a full-copper-span conductor elsewhere on
                // the net (another through via, a PTH pad) would otherwise register a bogus
                // connection on every layer and corrupt the measurement. Zones/pad landings are
                // deliberately not counted, so this can under-report but never false-positive.
                std::vector<BOARD_CONNECTED_ITEM*> landings = aConnectivity->GetConnectedItemsAtAnchor(
                        connItem, connItem->GetPosition(), { PCB_TRACE_T, PCB_ARC_T } );

                LSET span = connItem->GetLayerSet() & aCopper;
                int  depthToEnd = aStackup.GetLayerDistance( aDrill.start, aDrill.end );
                int  depthToNearest = std::numeric_limits<int>::max();
                bool haveConnection = false;

                for( BOARD_CONNECTED_ITEM* other : landings )
                {
                    if( other == connItem )
                        continue;

                    LSET otherCu = other->GetLayerSet();

                    for( PCB_LAYER_ID layer : span.CuStack() )
                    {
                        if( !otherCu.Contains( layer ) )
                            continue;

                        int depth = aStackup.GetLayerDistance( aDrill.start, layer );

                        // Only connections deeper than the must-cut are on the stub side.
                        if( depth > depthToEnd )
                        {
                            haveConnection = true;
                            depthToNearest = std::min( depthToNearest, depth );
                        }
                    }
                }

                if( !haveConnection )
                    return std::nullopt;

                return depthToNearest - depthToEnd;
            };

    auto checkStub =
            [&]( const PADSTACK::DRILL_PROPS& aDrill )
            {
                if( m_drcEngine->IsErrorLimitExceeded( DRCE_BACKDRILL_STUB_TOO_LONG ) )
                    return;

                std::optional<int> residual = residualStub( aDrill );

                if( !residual )
                    return;

                DRC_CONSTRAINT constraint = m_drcEngine->EvalRules(
                        BACKDRILL_STUB_LENGTH_CONSTRAINT, aItem, nullptr, aDrill.start );

                if( constraint.GetSeverity() == RPT_SEVERITY_IGNORE
                    || !constraint.Value().HasMax() )
                {
                    return;
                }

                if( *residual > constraint.Value().Max() )
                {
                    std::shared_ptr<DRC_ITEM> drcItem =
                            DRC_ITEM::Create( DRCE_BACKDRILL_STUB_TOO_LONG );

                    drcItem->SetErrorDetail( formatMsg( _( "(%s max backdrill stub %s; actual %s)" ),
                                                        constraint.GetName(),
                                                        constraint.Value().Max(),
                                                        *residual ) );
                    drcItem->SetItems( aItem );
                    drcItem->SetViolatingRule( constraint.GetParentRule() );
                    reportViolation( drcItem, aItem->GetPosition(), aDrill.start );
                }
            };

    auto checkDrill =
            [&]( const PADSTACK::DRILL_PROPS& aDrill )
            {
                // An empty backdrill slot (no layers, no size) is simply "no backdrill".
                if( aDrill.start == UNDEFINED_LAYER && aDrill.end == UNDEFINED_LAYER
                    && aDrill.size.x <= 0 )
                {
                    return;
                }

                bool invalid = false;

                if( !m_drcEngine->IsErrorLimitExceeded( DRCE_BACKDRILL_INVALID_SPAN ) )
                    invalid = reportInvalidSpan( aDrill );

                // A structurally-invalid span has no meaningful stub geometry to measure.
                // residualStub() re-validates the span, so this is also safe when the invalid-span
                // error limit has been reached and reportInvalidSpan was skipped.
                if( !invalid && aCheckStub )
                    checkStub( aDrill );
            };

    checkDrill( aPadstack.SecondaryDrill() );
    checkDrill( aPadstack.TertiaryDrill() );
}


bool DRC_TEST_PROVIDER_BACKDRILL::Run()
{
    bool checkInvalid = !m_drcEngine->IsErrorLimitExceeded( DRCE_BACKDRILL_INVALID_SPAN );
    bool checkStub = !m_drcEngine->IsErrorLimitExceeded( DRCE_BACKDRILL_STUB_TOO_LONG )
                     && m_drcEngine->HasRulesForConstraintType( BACKDRILL_STUB_LENGTH_CONSTRAINT );

    if( !checkInvalid && !checkStub )
    {
        REPORT_AUX( wxT( "Backdrill violations ignored. Tests not run." ) );
        return true; // continue with other tests
    }

    if( !reportPhase( _( "Checking backdrills..." ) ) )
        return false; // DRC cancelled

    BOARD*               board = m_drcEngine->GetBoard();
    LSET                 copper = LSET::AllCuMask( board->GetCopperLayerCount() );
    const BOARD_STACKUP& stackup = board->GetDesignSettings().GetStackupDescriptor();
    CONNECTIVITY_DATA*   connectivity = board->GetConnectivity().get();

    for( PCB_TRACK* track : board->Tracks() )
    {
        if( m_drcEngine->IsCancelled() )
            break;

        if( track->Type() != PCB_VIA_T )
            continue;

        PCB_VIA* via = static_cast<PCB_VIA*>( track );
        checkBackdrills( via, via->Padstack(), via->GetDrillValue(), copper, stackup, connectivity,
                         checkStub );
    }

    for( FOOTPRINT* footprint : board->Footprints() )
    {
        for( PAD* pad : footprint->Pads() )
        {
            if( m_drcEngine->IsCancelled() )
                break;

            checkBackdrills( pad, pad->Padstack(), pad->GetDrillSizeX(), copper, stackup,
                             connectivity, checkStub );
        }
    }

    return !m_drcEngine->IsCancelled();
}


namespace detail
{
static DRC_REGISTER_TEST_PROVIDER<DRC_TEST_PROVIDER_BACKDRILL> dummy;
}
