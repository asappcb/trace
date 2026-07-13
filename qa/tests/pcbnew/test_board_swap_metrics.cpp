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

/*
 * Unit tests for EstimateSwapRatsnestDelta (crossing/length-delta preview metric, epic #20 P1):
 * the geometric MST-length change over the affected nets under a hypothetical net reassignment.
 */

#include <boost/test/unit_test.hpp>

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <board.h>
#include <board_swap_metrics.h>
#include <gate_swap.h>
#include <footprint.h>
#include <netinfo.h>
#include <pad.h>
#include <pcb_track.h>
#include <layer_ids.h>
#include <base_units.h>

#include <cmath>
#include <map>
#include <memory>


namespace
{
PAD* addPad( FOOTPRINT* aFp, NETINFO_ITEM* aNet, double aXmm, double aYmm )
{
    PAD* pad = new PAD( aFp );
    pad->SetNumber( wxString::Format( wxT( "%d" ), (int) aFp->Pads().size() + 1 ) );
    pad->SetPosition( VECTOR2I( pcbIUScale.mmToIU( aXmm ), pcbIUScale.mmToIU( aYmm ) ) );
    pad->SetNet( aNet );
    aFp->Add( pad );
    return pad;
}


PAD* addNumberedPad( FOOTPRINT* aFp, const wxString& aNumber, NETINFO_ITEM* aNet, double aXmm,
                     double aYmm )
{
    PAD* pad = new PAD( aFp );
    pad->SetNumber( aNumber );
    pad->SetPosition( VECTOR2I( pcbIUScale.mmToIU( aXmm ), pcbIUScale.mmToIU( aYmm ) ) );
    pad->SetNet( aNet );
    aFp->Add( pad );
    return pad;
}
} // namespace


BOOST_AUTO_TEST_SUITE( BoardSwapMetrics )


// Four pads on two nets, laid out so the current assignment routes "across" (long) and swapping two
// pads groups each net locally (short). The delta must be strongly negative and equal the exact
// geometric improvement.
BOOST_AUTO_TEST_CASE( SwapShorteningRatsnestGivesNegativeDelta )
{
    std::unique_ptr<BOARD> board = std::make_unique<BOARD>();

    NETINFO_ITEM* net1 = new NETINFO_ITEM( board.get(), wxT( "N1" ), 1 );
    NETINFO_ITEM* net2 = new NETINFO_ITEM( board.get(), wxT( "N2" ), 2 );
    board->Add( net1 );
    board->Add( net2 );

    FOOTPRINT* fp = new FOOTPRINT( board.get() );
    board->Add( fp );

    // A(0,0)=N1  C(1,0)=N2  B(10,0)=N2  D(11,0)=N1
    // before: N1 = {A,D} len 11mm; N2 = {B,C} len 9mm; total 20mm.
    PAD* a = addPad( fp, net1, 0.0, 0.0 );
    PAD* c = addPad( fp, net2, 1.0, 0.0 );
    PAD* b = addPad( fp, net2, 10.0, 0.0 );
    PAD* d = addPad( fp, net1, 11.0, 0.0 );
    (void) a;
    (void) b;

    // Swap C<->D nets: N1 = {A,C} len 1mm; N2 = {B,D} len 1mm; total 2mm. Delta = 2 - 20 = -18mm.
    std::map<const PAD*, int> swap;
    swap[c] = 1; // C joins N1
    swap[d] = 2; // D joins N2

    double delta = EstimateSwapRatsnestDelta( board.get(), swap );

    BOOST_CHECK( delta < 0.0 );
    BOOST_CHECK_CLOSE( delta, -static_cast<double>( pcbIUScale.mmToIU( 18.0 ) ), 0.1 );
}


// The reverse: starting from the grouped-and-short layout, swapping to the crossed layout must give
// the exact positive (worsening) delta.
BOOST_AUTO_TEST_CASE( SwapLengtheningRatsnestGivesPositiveDelta )
{
    std::unique_ptr<BOARD> board = std::make_unique<BOARD>();

    NETINFO_ITEM* net1 = new NETINFO_ITEM( board.get(), wxT( "N1" ), 1 );
    NETINFO_ITEM* net2 = new NETINFO_ITEM( board.get(), wxT( "N2" ), 2 );
    board->Add( net1 );
    board->Add( net2 );

    FOOTPRINT* fp = new FOOTPRINT( board.get() );
    board->Add( fp );

    // Grouped: A(0,0)=N1 C(1,0)=N1 B(10,0)=N2 D(11,0)=N2 -> total 2mm.
    PAD* a = addPad( fp, net1, 0.0, 0.0 );
    PAD* c = addPad( fp, net1, 1.0, 0.0 );
    PAD* b = addPad( fp, net2, 10.0, 0.0 );
    PAD* d = addPad( fp, net2, 11.0, 0.0 );
    (void) a;
    (void) b;

    // Swap C<->B nets -> crossed layout, total 20mm. Delta = +18mm.
    std::map<const PAD*, int> swap;
    swap[c] = 2;
    swap[b] = 1;

    double delta = EstimateSwapRatsnestDelta( board.get(), swap );

    BOOST_CHECK( delta > 0.0 );
    BOOST_CHECK_CLOSE( delta, static_cast<double>( pcbIUScale.mmToIU( 18.0 ) ), 0.1 );
}


// Exercises MST over 3+ points (the Prim relaxation loop) and a net whose pad count drops to 1
// (length 0) as a result of the swap.
BOOST_AUTO_TEST_CASE( ThreePadNetAndNetShrinkingToOnePad )
{
    std::unique_ptr<BOARD> board = std::make_unique<BOARD>();

    NETINFO_ITEM* net1 = new NETINFO_ITEM( board.get(), wxT( "N1" ), 1 );
    NETINFO_ITEM* net2 = new NETINFO_ITEM( board.get(), wxT( "N2" ), 2 );
    board->Add( net1 );
    board->Add( net2 );

    FOOTPRINT* fp = new FOOTPRINT( board.get() );
    board->Add( fp );

    // N1 = { (0,0), (3,0), (3,4) } -> MST = 3 + 4 = 7mm (the 5mm hypotenuse is never chosen).
    // N2 = { (100,0) } -> a lone pad, MST length 0.
    PAD* p0 = addPad( fp, net1, 0.0, 0.0 );
    PAD* p1 = addPad( fp, net1, 3.0, 0.0 );
    PAD* p2 = addPad( fp, net1, 3.0, 4.0 );
    PAD* lone = addPad( fp, net2, 100.0, 0.0 );
    (void) p0;
    (void) p1;

    // Move the far corner (3,4) from N1 to N2. N1 becomes {(0,0),(3,0)} = 3mm; N2 becomes
    // {(100,0),(3,4)} = ~97.08mm. Delta = (3 + 97.08...) - (7 + 0).
    std::map<const PAD*, int> swap;
    swap[p2] = 2;

    double n1Before = 7.0;
    double n1After = 3.0;
    double n2Before = 0.0;
    double n2After = std::hypot( 100.0 - 3.0, 0.0 - 4.0 ); // ~97.0824mm
    double expectedMm = ( n1After + n2After ) - ( n1Before + n2Before );

    double delta = EstimateSwapRatsnestDelta( board.get(), swap );

    BOOST_CHECK( delta > 0.0 ); // the far move lengthens overall
    BOOST_CHECK_CLOSE( delta, expectedMm * pcbIUScale.IU_PER_MM, 0.1 );
    (void) lone;
}


// Guards: an empty reassignment and a null board both yield a zero delta.
BOOST_AUTO_TEST_CASE( EmptyOrNullYieldsZeroDelta )
{
    std::unique_ptr<BOARD> board = std::make_unique<BOARD>();

    BOOST_CHECK_EQUAL( EstimateSwapRatsnestDelta( board.get(), {} ), 0.0 );
    BOOST_CHECK_EQUAL( EstimateSwapRatsnestDelta( nullptr, {} ), 0.0 );
}


// Build a board with a 2-gate interchangeable part whose gates are "crossed": gate A (near net1's
// anchor) currently carries net2 (far), gate B (near net2's anchor) carries net1 (far). The best
// swap regroups each net locally. @p aInterchangeable controls the units-locked gate.
static std::unique_ptr<BOARD> makeCrossedTwoGateBoard( bool aInterchangeable, FOOTPRINT** aOutFp )
{
    auto board = std::make_unique<BOARD>();

    NETINFO_ITEM* net1 = new NETINFO_ITEM( board.get(), wxT( "N1" ), 1 );
    NETINFO_ITEM* net2 = new NETINFO_ITEM( board.get(), wxT( "N2" ), 2 );
    board->Add( net1 );
    board->Add( net2 );

    // External anchors: net1 near x=0, net2 near x=100.
    FOOTPRINT* ext = new FOOTPRINT( board.get() );
    ext->SetReference( wxT( "J1" ) );
    board->Add( ext );
    addNumberedPad( ext, wxT( "1" ), net1, 0.0, 0.0 );
    addNumberedPad( ext, wxT( "2" ), net2, 100.0, 0.0 );

    // The gate part: gate A pads near x=0 but on net2; gate B pads near x=100 but on net1 (crossed).
    FOOTPRINT* fp = new FOOTPRINT( board.get() );
    fp->SetReference( wxT( "U1" ) );
    board->Add( fp );
    addNumberedPad( fp, wxT( "1" ), net2, 5.0, 0.0 );   // gate A
    addNumberedPad( fp, wxT( "2" ), net2, 6.0, 0.0 );   // gate A
    addNumberedPad( fp, wxT( "3" ), net1, 105.0, 0.0 ); // gate B
    addNumberedPad( fp, wxT( "4" ), net1, 106.0, 0.0 ); // gate B

    fp->SetUnitInfo( { { wxT( "A" ), { wxT( "1" ), wxT( "2" ) } },
                       { wxT( "B" ), { wxT( "3" ), wxT( "4" ) } } } );
    fp->SetUnitsInterchangeable( aInterchangeable );

    if( aOutFp )
        *aOutFp = fp;

    return board;
}


// The planner must find the improving A<->B gate swap on a crossed interchangeable part.
BOOST_AUTO_TEST_CASE( PlannerFindsImprovingGateSwap )
{
    FOOTPRINT*             fp = nullptr;
    std::unique_ptr<BOARD> board = makeCrossedTwoGateBoard( true, &fp );

    std::vector<GATE_SWAP_PLAN_ITEM> plan = PlanGateSwapOptimization( board.get() );

    BOOST_REQUIRE_EQUAL( plan.size(), 1u );
    BOOST_CHECK_EQUAL( plan[0].m_footprint, fp );
    // The only interchangeable pair is unit index 0 (A) and 1 (B), in either order.
    BOOST_CHECK( ( plan[0].m_unitIndexA == 0 && plan[0].m_unitIndexB == 1 )
                 || ( plan[0].m_unitIndexA == 1 && plan[0].m_unitIndexB == 0 ) );

    // Exact reduction: before {net1 106mm, net2 95mm}=201; after {6mm,6mm}=12; delta = -189mm.
    BOOST_CHECK_CLOSE( plan[0].m_delta, -static_cast<double>( pcbIUScale.mmToIU( 189.0 ) ), 0.1 );
}


// Three interchangeable gates arranged as a 3-cycle (each gate carries a net whose anchor is at a
// different gate's home). Fixing it needs TWO sequential swaps, so this exercises the greedy
// composition (each step's delta measured against the state left by the previous step).
BOOST_AUTO_TEST_CASE( PlannerComposesMultipleSwaps )
{
    std::unique_ptr<BOARD> board = std::make_unique<BOARD>();

    NETINFO_ITEM* net1 = new NETINFO_ITEM( board.get(), wxT( "N1" ), 1 );
    NETINFO_ITEM* net2 = new NETINFO_ITEM( board.get(), wxT( "N2" ), 2 );
    NETINFO_ITEM* net3 = new NETINFO_ITEM( board.get(), wxT( "N3" ), 3 );
    board->Add( net1 );
    board->Add( net2 );
    board->Add( net3 );

    // Anchors: net1 home x=0, net2 home x=10, net3 home x=20.
    FOOTPRINT* ext = new FOOTPRINT( board.get() );
    ext->SetReference( wxT( "J1" ) );
    board->Add( ext );
    addNumberedPad( ext, wxT( "1" ), net1, 0.0, 0.0 );
    addNumberedPad( ext, wxT( "2" ), net2, 10.0, 0.0 );
    addNumberedPad( ext, wxT( "3" ), net3, 20.0, 0.0 );

    // Gate part: gate A near x=0, gate B near x=10, gate C near x=20; but the nets are 3-cycled:
    // A carries net2, B carries net3, C carries net1.
    FOOTPRINT* fp = nullptr;
    fp = new FOOTPRINT( board.get() );
    fp->SetReference( wxT( "U1" ) );
    board->Add( fp );
    addNumberedPad( fp, wxT( "1" ), net2, 0.1, 0.0 );  // gate A (home net1)
    addNumberedPad( fp, wxT( "2" ), net3, 10.1, 0.0 ); // gate B (home net2)
    addNumberedPad( fp, wxT( "3" ), net1, 20.1, 0.0 ); // gate C (home net3)

    fp->SetUnitInfo( { { wxT( "A" ), { wxT( "1" ) } },
                       { wxT( "B" ), { wxT( "2" ) } },
                       { wxT( "C" ), { wxT( "3" ) } } } );
    fp->SetUnitsInterchangeable( true );

    std::vector<GATE_SWAP_PLAN_ITEM> plan = PlanGateSwapOptimization( board.get() );

    // A 3-cycle needs two transpositions to resolve.
    BOOST_REQUIRE_EQUAL( plan.size(), 2u );
    BOOST_CHECK( plan[0].m_delta < 0.0 );
    BOOST_CHECK( plan[1].m_delta < 0.0 );

    // Replay the plan by unit index (exchange the two gates' single pads) and confirm the board is
    // then optimal: a re-plan finds nothing further. This proves the composed sequence is real.
    auto padOfUnitIndex = [&]( int aIndex ) -> PAD*
    {
        return fp->FindPadByNumber( fp->GetUnitInfo()[aIndex].m_pins.front() );
    };

    for( const GATE_SWAP_PLAN_ITEM& step : plan )
    {
        PAD* pa = padOfUnitIndex( step.m_unitIndexA );
        PAD* pb = padOfUnitIndex( step.m_unitIndexB );
        BOOST_REQUIRE( pa && pb );
        int na = pa->GetNetCode();
        int nb = pb->GetNetCode();
        pa->SetNetCode( nb );
        pb->SetNetCode( na );
    }

    auto padOfUnit = [&]( const wxString& aUnit ) -> PAD*
    {
        for( const FOOTPRINT::FP_UNIT_INFO& u : fp->GetUnitInfo() )
        {
            if( u.m_unitName == aUnit )
                return fp->FindPadByNumber( u.m_pins.front() );
        }
        return nullptr;
    };

    // Each gate should now sit on the net whose anchor is at its home position.
    BOOST_CHECK_EQUAL( padOfUnit( wxT( "A" ) )->GetNetCode(), 1 );
    BOOST_CHECK_EQUAL( padOfUnit( wxT( "B" ) )->GetNetCode(), 2 );
    BOOST_CHECK_EQUAL( padOfUnit( wxT( "C" ) )->GetNetCode(), 3 );

    BOOST_CHECK( PlanGateSwapOptimization( board.get() ).empty() );
}


// A part whose units are locked (not interchangeable) must be skipped entirely.
BOOST_AUTO_TEST_CASE( PlannerSkipsLockedUnits )
{
    std::unique_ptr<BOARD> board = makeCrossedTwoGateBoard( false, nullptr );

    BOOST_CHECK( PlanGateSwapOptimization( board.get() ).empty() );
}


// Applying the planned swap (reassigning the gate pads' nets) leaves nothing further to improve --
// the planner converges and re-planning the optimized board yields an empty plan.
BOOST_AUTO_TEST_CASE( PlannerConvergesAfterApplying )
{
    FOOTPRINT*             fp = nullptr;
    std::unique_ptr<BOARD> board = makeCrossedTwoGateBoard( true, &fp );

    // Apply the A<->B swap by hand: exchange the nets of pads (1,2) and (3,4).
    PAD* p1 = fp->FindPadByNumber( wxT( "1" ) );
    PAD* p2 = fp->FindPadByNumber( wxT( "2" ) );
    PAD* p3 = fp->FindPadByNumber( wxT( "3" ) );
    PAD* p4 = fp->FindPadByNumber( wxT( "4" ) );

    int n1 = p1->GetNetCode();
    int n3 = p3->GetNetCode();
    p1->SetNetCode( n3 );
    p2->SetNetCode( n3 );
    p3->SetNetCode( n1 );
    p4->SetNetCode( n1 );

    BOOST_CHECK( PlanGateSwapOptimization( board.get() ).empty() );
}


// The headless applier (ApplyGateSwapPlan) must actually reassign the gate pads' net codes to the
// planned optimal assignment -- the frame-independent path the CLI/batch optimizer uses.
BOOST_AUTO_TEST_CASE( ApplyPlanReassignsGatePadNets )
{
    FOOTPRINT*             fp = nullptr;
    std::unique_ptr<BOARD> board = makeCrossedTwoGateBoard( true, &fp );
    board->BuildConnectivity();

    std::vector<GATE_SWAP_PLAN_ITEM> plan = PlanGateSwapOptimization( board.get() );
    BOOST_REQUIRE_EQUAL( plan.size(), 1u );

    int applied = ApplyGateSwapPlan( board.get(), plan );
    BOOST_CHECK_EQUAL( applied, 1 );

    // Gate A (pads 1,2) regroups onto net1; gate B (pads 3,4) onto net2.
    BOOST_CHECK_EQUAL( fp->FindPadByNumber( wxT( "1" ) )->GetNetCode(), 1 );
    BOOST_CHECK_EQUAL( fp->FindPadByNumber( wxT( "2" ) )->GetNetCode(), 1 );
    BOOST_CHECK_EQUAL( fp->FindPadByNumber( wxT( "3" ) )->GetNetCode(), 2 );
    BOOST_CHECK_EQUAL( fp->FindPadByNumber( wxT( "4" ) )->GetNetCode(), 2 );

    // The applied board is optimal: re-planning finds nothing further.
    board->BuildConnectivity();
    BOOST_CHECK( PlanGateSwapOptimization( board.get() ).empty() );
}


// The connected-copper propagation must follow the swap: a track sitting on a gate pad's net gets
// reassigned along with the pad. This exercises ComputeGateSwapNetChanges' non-pad m_always path,
// which the unrouted board above never hits.
BOOST_AUTO_TEST_CASE( ApplyPlanPropagatesToConnectedTracks )
{
    FOOTPRINT*             fp = nullptr;
    std::unique_ptr<BOARD> board = makeCrossedTwoGateBoard( true, &fp );

    PAD*      padA1 = fp->FindPadByNumber( wxT( "1" ) );
    const int gateAnet = padA1->GetNetCode(); // net2, gate A's crossed net

    // A track on gate A's net, starting on pad 1 so connectivity attaches it to the gate.
    PCB_TRACK* track = new PCB_TRACK( board.get() );
    track->SetStart( padA1->GetPosition() );
    track->SetEnd( padA1->GetPosition() + VECTOR2I( 0, pcbIUScale.mmToIU( 5.0 ) ) );
    track->SetLayer( F_Cu );
    track->SetNetCode( gateAnet );
    board->Add( track );

    board->BuildConnectivity();

    std::vector<GATE_SWAP_PLAN_ITEM> plan = PlanGateSwapOptimization( board.get() );
    BOOST_REQUIRE_EQUAL( plan.size(), 1u );

    BOOST_CHECK_EQUAL( ApplyGateSwapPlan( board.get(), plan ), 1 );

    // Gate A pad 1 regroups onto net1, and its connected track follows.
    BOOST_CHECK_EQUAL( padA1->GetNetCode(), 1 );
    BOOST_CHECK_EQUAL( track->GetNetCode(), 1 );
}


// Applying an empty plan is a no-op; a null board is handled.
BOOST_AUTO_TEST_CASE( ApplyPlanGuards )
{
    std::unique_ptr<BOARD> board = std::make_unique<BOARD>();
    BOOST_CHECK_EQUAL( ApplyGateSwapPlan( board.get(), {} ), 0 );
    BOOST_CHECK_EQUAL( ApplyGateSwapPlan( nullptr, {} ), 0 );
}


BOOST_AUTO_TEST_SUITE_END()
