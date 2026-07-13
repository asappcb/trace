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
#include <footprint.h>
#include <netinfo.h>
#include <pad.h>
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


BOOST_AUTO_TEST_SUITE_END()
