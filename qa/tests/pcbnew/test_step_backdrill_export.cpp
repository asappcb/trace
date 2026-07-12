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
 * OCC/STEP export regression coverage for back-drilling (epic #13, item #24.2).
 *
 * Two properties are checked without depending on the exact default-stackup Z values:
 *  - GetCopperLayerKnockouts() gates copper layers by the feature's Z extent (a shallow front
 *    feature knocks out only F_Cu; a full-depth feature knocks out every copper layer) and returns
 *    the correct per-layer diameters (constant for a counterbore, tapering for a countersink).
 *  - A backdrilled board is accepted by the OCC modeller (AddBackdrill + CreatePCB) and writes a
 *    non-empty STEP file.
 */

#include <filesystem>

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <boost/test/unit_test.hpp>

#include <board.h>
#include <board_design_settings.h>
#include <board_stackup_manager/board_stackup.h>
#include <footprint.h> // defines EXTRUSION_MATERIAL, used by step_pcb_model.h
#include <reporter.h>
#include <layer_ids.h>
#include <base_units.h>
#include <geometry/shape_poly_set.h>
#include <geometry/shape_segment.h>

#include <exporters/step/step_pcb_model.h>

#include <map>


BOOST_AUTO_TEST_SUITE( StepBackdrillExport )


// The default 6-layer stackup, so copper Z placement is well defined (F_Cu at the top surface,
// B_Cu at the bottom, In1..In4 between). Returned by value: BOARD_STACKUP deep-copies safely.
// STEP_PCB_MODEL owns an OCC document handle and is not safely movable, so it is constructed
// in-place in each test rather than returned from a helper.
static BOARD_STACKUP makeSixLayerStackup()
{
    BOARD board;
    board.SetCopperLayerCount( 6 );
    return board.GetStackupOrDefault();
}


// A counterbore removes a constant-diameter recess; the affected copper layers are exactly those
// within the feature's Z extent from the chosen surface.
BOOST_AUTO_TEST_CASE( CounterboreKnockoutZExtent )
{
    NULL_REPORTER reporter;
    STEP_PCB_MODEL model( wxT( "backdrill_test" ), &reporter );
    model.SetStackup( makeSixLayerStackup() );

    const int diameter = pcbIUScale.mmToIU( 0.80 );
    const int shallow = pcbIUScale.mmToIU( 0.001 ); // reaches only the surface copper
    const int deep = pcbIUScale.mmToIU( 10.0 );     // reaches through the whole board

    // Front, shallow: only F_Cu, at the full counterbore diameter.
    std::map<PCB_LAYER_ID, int> frontShallow = model.GetCopperLayerKnockouts( diameter, shallow, 0,
                                                                              true );
    BOOST_CHECK_EQUAL( frontShallow.size(), 1 );
    BOOST_REQUIRE( frontShallow.count( F_Cu ) );
    BOOST_CHECK_EQUAL( frontShallow[F_Cu], diameter );

    // Back, shallow: only B_Cu.
    std::map<PCB_LAYER_ID, int> backShallow = model.GetCopperLayerKnockouts( diameter, shallow, 0,
                                                                             false );
    BOOST_CHECK_EQUAL( backShallow.size(), 1 );
    BOOST_CHECK( backShallow.count( B_Cu ) );

    // Front, full depth: every copper layer, each at the constant counterbore diameter.
    std::map<PCB_LAYER_ID, int> deepAll = model.GetCopperLayerKnockouts( diameter, deep, 0, true );
    BOOST_CHECK_EQUAL( deepAll.size(), 6 );
    BOOST_REQUIRE( deepAll.count( F_Cu ) );
    BOOST_REQUIRE( deepAll.count( B_Cu ) );

    for( const auto& [layer, dia] : deepAll )
        BOOST_CHECK_EQUAL( dia, diameter );
}


// A countersink tapers: the entry layer gets the full diameter and deeper layers get progressively
// smaller diameters, all bounded by the nominal diameter.
BOOST_AUTO_TEST_CASE( CountersinkKnockoutTapers )
{
    NULL_REPORTER reporter;
    STEP_PCB_MODEL model( wxT( "backdrill_test" ), &reporter );
    model.SetStackup( makeSixLayerStackup() );

    const int diameter = pcbIUScale.mmToIU( 8.0 );  // wide enough that the cone spans the board
    const int depth = pcbIUScale.mmToIU( 10.0 );    // reach every layer
    const int angle = 300;                          // 30 deg, in 0.1-deg units; gentle taper

    std::map<PCB_LAYER_ID, int> knockouts = model.GetCopperLayerKnockouts( diameter, depth, angle,
                                                                           true );

    BOOST_REQUIRE( knockouts.count( F_Cu ) );
    BOOST_REQUIRE( knockouts.count( B_Cu ) );

    // Entry layer is at (essentially) the nominal diameter and is the widest knockout.
    BOOST_CHECK( knockouts[F_Cu] <= diameter );
    BOOST_CHECK( knockouts[F_Cu] >= diameter - pcbIUScale.mmToIU( 0.01 ) );

    // The far layer is strictly narrower than the entry layer (the cone tapers with depth).
    BOOST_CHECK( knockouts[B_Cu] < knockouts[F_Cu] );

    for( const auto& [layer, dia] : knockouts )
    {
        BOOST_CHECK( dia > 0 );
        BOOST_CHECK( dia <= diameter );
    }

    // F_Cu is the widest: no layer exceeds it.
    for( const auto& [layer, dia] : knockouts )
        BOOST_CHECK( dia <= knockouts[F_Cu] );
}


// A backdrilled board must be accepted by the OCC modeller and produce a non-empty STEP file. The
// backdrill removes board material and copper between its start/end layers (AddBackdrill), which
// CreatePCB then subtracts from the board body.
BOOST_AUTO_TEST_CASE( BackdrillGeometryExportsToStep )
{
    NULL_REPORTER reporter;
    STEP_PCB_MODEL model( wxT( "backdrill_test" ), &reporter );
    model.SetStackup( makeSixLayerStackup() );
    model.SpecializeVariant( OUTPUT_FORMAT::FMT_OUT_STEP );

    // A 10 mm square board outline.
    SHAPE_POLY_SET outline;
    outline.NewOutline();
    outline.Append( VECTOR2I( 0, 0 ) );
    outline.Append( VECTOR2I( pcbIUScale.mmToIU( 10.0 ), 0 ) );
    outline.Append( VECTOR2I( pcbIUScale.mmToIU( 10.0 ), pcbIUScale.mmToIU( 10.0 ) ) );
    outline.Append( VECTOR2I( 0, pcbIUScale.mmToIU( 10.0 ) ) );

    // A top backdrill: a round hole (zero-length segment) 0.4 mm across, from F_Cu to In2_Cu.
    VECTOR2I center( pcbIUScale.mmToIU( 5.0 ), pcbIUScale.mmToIU( 5.0 ) );
    SHAPE_SEGMENT backdrill( center, center, pcbIUScale.mmToIU( 0.40 ) );

    BOOST_REQUIRE( model.AddBackdrill( backdrill, F_Cu, In2_Cu, VECTOR2D( 0, 0 ) ) );
    BOOST_REQUIRE( model.CreatePCB( outline, VECTOR2D( 0, 0 ), true ) );

    const std::filesystem::path out =
            std::filesystem::temp_directory_path() / "kicad_step_backdrill_test.step";

    if( std::filesystem::exists( out ) )
        std::filesystem::remove( out );

    const wxString outPath = wxString::FromUTF8( out.string().c_str() );
    BOOST_REQUIRE( model.WriteSTEP( outPath, false, false ) );

    BOOST_REQUIRE( std::filesystem::exists( out ) );
    BOOST_CHECK( std::filesystem::file_size( out ) > 0 );

    std::error_code ec;
    std::filesystem::remove( out, ec );
}


BOOST_AUTO_TEST_SUITE_END()
