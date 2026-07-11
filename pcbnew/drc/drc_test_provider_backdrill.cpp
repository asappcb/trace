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

#include <board.h>
#include <footprint.h>
#include <pad.h>
#include <pcb_track.h>
#include <padstack.h>
#include <layer_ids.h>
#include <lset.h>
#include <drc/drc_engine.h>
#include <drc/drc_item.h>
#include <drc/drc_test_provider.h>

/*
    Backdrill validity test.

    Checks that every backdrill (secondary/tertiary drill on a via or pad padstack) describes a
    manufacturable span: it starts at an outer copper layer, its start and must-cut layers are
    enabled copper layers, the two are different, and the backdrill diameter is not smaller than
    the primary drill it is meant to clear.

    Errors generated:
    - DRCE_BACKDRILL_INVALID_SPAN
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
                          const LSET& aCopper );
};


void DRC_TEST_PROVIDER_BACKDRILL::checkBackdrills( BOARD_ITEM* aItem, const PADSTACK& aPadstack,
                                                   int aPrimaryDrill, const LSET& aCopper )
{
    auto checkDrill =
            [&]( const PADSTACK::DRILL_PROPS& aDrill )
            {
                // An empty backdrill slot (no layers, no size) is simply "no backdrill".
                if( aDrill.start == UNDEFINED_LAYER && aDrill.end == UNDEFINED_LAYER
                    && aDrill.size.x <= 0 )
                {
                    return;
                }

                if( m_drcEngine->IsErrorLimitExceeded( DRCE_BACKDRILL_INVALID_SPAN ) )
                    return;

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
                    return; // valid

                std::shared_ptr<DRC_ITEM> drcItem = DRC_ITEM::Create( DRCE_BACKDRILL_INVALID_SPAN );
                drcItem->SetErrorDetail( msg );
                drcItem->SetItems( aItem );

                PCB_LAYER_ID markerLayer = aDrill.start != UNDEFINED_LAYER ? aDrill.start : F_Cu;
                reportViolation( drcItem, aItem->GetPosition(), markerLayer );
            };

    checkDrill( aPadstack.SecondaryDrill() );
    checkDrill( aPadstack.TertiaryDrill() );
}


bool DRC_TEST_PROVIDER_BACKDRILL::Run()
{
    if( m_drcEngine->IsErrorLimitExceeded( DRCE_BACKDRILL_INVALID_SPAN ) )
    {
        REPORT_AUX( wxT( "Backdrill validity violations ignored. Tests not run." ) );
        return true; // continue with other tests
    }

    if( !reportPhase( _( "Checking backdrills..." ) ) )
        return false; // DRC cancelled

    BOARD* board = m_drcEngine->GetBoard();
    LSET   copper = LSET::AllCuMask( board->GetCopperLayerCount() );

    for( PCB_TRACK* track : board->Tracks() )
    {
        if( m_drcEngine->IsErrorLimitExceeded( DRCE_BACKDRILL_INVALID_SPAN ) )
            break;

        if( track->Type() != PCB_VIA_T )
            continue;

        PCB_VIA* via = static_cast<PCB_VIA*>( track );
        checkBackdrills( via, via->Padstack(), via->GetDrillValue(), copper );
    }

    for( FOOTPRINT* footprint : board->Footprints() )
    {
        for( PAD* pad : footprint->Pads() )
        {
            if( m_drcEngine->IsErrorLimitExceeded( DRCE_BACKDRILL_INVALID_SPAN ) )
                break;

            checkBackdrills( pad, pad->Padstack(), pad->GetDrillSizeX(), copper );
        }
    }

    return !m_drcEngine->IsCancelled();
}


namespace detail
{
static DRC_REGISTER_TEST_PROVIDER<DRC_TEST_PROVIDER_BACKDRILL> dummy;
}
