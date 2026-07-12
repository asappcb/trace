/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
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

/**
 * @file test_drc_backdrill_postmachining.cpp
 * Tests for DRC and connectivity checks related to backdrilling and post-machining
 */

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <pcbnew_utils/board_test_utils.h>

#include <board.h>
#include <board_design_settings.h>
#include <board_stackup_manager/board_stackup.h>
#include <connectivity/connectivity_data.h>
#include <drc/drc_engine.h>
#include <drc/drc_item.h>
#include <footprint.h>
#include <pad.h>
#include <pcb_track.h>
#include <pcb_marker.h>
#include <settings/settings_manager.h>
#include <zone.h>
#include <zone_filler.h>

#include <filesystem>
#include <fstream>


struct BACKDRILL_TEST_FIXTURE
{
    BACKDRILL_TEST_FIXTURE()
    {
        m_board = std::make_unique<BOARD>();
        SetupSixLayerBoard();
    }

    void SetupSixLayerBoard()
    {
        // Set up a 6-layer board with proper stackup for layer distance calculations
        m_board->SetCopperLayerCount( 6 );
        m_board->SetEnabledLayers( m_board->GetEnabledLayers() | LSET::AllCuMask( 6 ) );

        BOARD_DESIGN_SETTINGS& bds = m_board->GetDesignSettings();
        bds.SetCopperLayerCount( 6 );

        // Set up a proper stackup with known layer thicknesses
        BOARD_STACKUP& stackup = bds.GetStackupDescriptor();
        stackup.BuildDefaultStackupList( &bds, 6 );

        // Build connectivity and DRC engine
        m_board->BuildConnectivity();

        auto drcEngine = std::make_shared<DRC_ENGINE>( m_board.get(), &bds );
        drcEngine->InitEngine( wxFileName() );
        bds.m_DRCEngine = drcEngine;
    }

    /**
     * Create a via with backdrill settings
     * @param aPos Position of the via
     * @param aNetCode Net code for the via
     * @param aPrimaryStart Start layer for primary drill
     * @param aPrimaryEnd End layer for primary drill
     * @param aSecondaryStart Start layer for backdrill (secondary drill)
     * @param aSecondaryEnd End layer for backdrill
     * @param aSecondaryDrillSize Size of the backdrill
     * @return Pointer to the created via
     */
    PCB_VIA* CreateBackdrilledVia( const VECTOR2I& aPos, int aNetCode,
                                   PCB_LAYER_ID aPrimaryStart, PCB_LAYER_ID aPrimaryEnd,
                                   PCB_LAYER_ID aSecondaryStart, PCB_LAYER_ID aSecondaryEnd,
                                   int aSecondaryDrillSize )
    {
        PCB_VIA* via = new PCB_VIA( m_board.get() );
        via->SetPosition( aPos );
        via->SetLayerPair( aPrimaryStart, aPrimaryEnd );
        via->SetDrill( pcbIUScale.mmToIU( 0.3 ) );
        via->SetWidth( PADSTACK::ALL_LAYERS, pcbIUScale.mmToIU( 0.6 ) );
        via->SetNetCode( aNetCode );
        via->SetSecondaryDrillSize( aSecondaryDrillSize );
        via->SetSecondaryDrillStartLayer( aSecondaryStart );
        via->SetSecondaryDrillEndLayer( aSecondaryEnd );
        m_board->Add( via );
        return via;
    }

    /**
     * Create a via with post-machining settings
     * @param aPos Position of the via
     * @param aNetCode Net code for the via
     * @param aFrontMode Post-machining mode for front (COUNTERBORE or COUNTERSINK)
     * @param aFrontSize Size of front post-machining
     * @param aFrontDepth Depth of front post-machining
     * @return Pointer to the created via
     */
    PCB_VIA* CreatePostMachinedVia( const VECTOR2I& aPos, int aNetCode,
                                    PAD_DRILL_POST_MACHINING_MODE aFrontMode,
                                    int aFrontSize, int aFrontDepth )
    {
        PCB_VIA* via = new PCB_VIA( m_board.get() );
        via->SetPosition( aPos );
        via->SetLayerPair( F_Cu, B_Cu );
        via->SetDrill( pcbIUScale.mmToIU( 0.3 ) );
        via->SetWidth( PADSTACK::ALL_LAYERS, pcbIUScale.mmToIU( 0.6 ) );
        via->SetNetCode( aNetCode );
        via->SetFrontPostMachiningMode( aFrontMode );
        via->SetFrontPostMachiningSize( aFrontSize );
        via->SetFrontPostMachiningDepth( aFrontDepth );
        if( aFrontMode == PAD_DRILL_POST_MACHINING_MODE::COUNTERSINK )
            via->SetFrontPostMachiningAngle( 900 ); // 90 degrees
        m_board->Add( via );
        return via;
    }

    /**
     * Create a simple track segment
     */
    PCB_TRACK* CreateTrack( const VECTOR2I& aStart, const VECTOR2I& aEnd,
                            PCB_LAYER_ID aLayer, int aNetCode )
    {
        PCB_TRACK* track = new PCB_TRACK( m_board.get() );
        track->SetStart( aStart );
        track->SetEnd( aEnd );
        track->SetLayer( aLayer );
        track->SetWidth( pcbIUScale.mmToIU( 0.25 ) );
        track->SetNetCode( aNetCode );
        m_board->Add( track );
        return track;
    }

    /**
     * Create a footprint with a PTH pad
     */
    FOOTPRINT* CreateFootprintWithPad( const VECTOR2I& aPos, int aNetCode,
                                       const wxString& aPadNumber = "1" )
    {
        FOOTPRINT* fp = new FOOTPRINT( m_board.get() );
        fp->SetPosition( aPos );
        fp->SetReference( "U1" );

        PAD* pad = new PAD( fp );
        pad->SetPosition( aPos );
        pad->SetNumber( aPadNumber );
        pad->SetShape( PADSTACK::ALL_LAYERS, PAD_SHAPE::CIRCLE );
        pad->SetSize( PADSTACK::ALL_LAYERS, VECTOR2I( pcbIUScale.mmToIU( 1.5 ), pcbIUScale.mmToIU( 1.5 ) ) );
        pad->SetDrillSize( VECTOR2I( pcbIUScale.mmToIU( 0.8 ), pcbIUScale.mmToIU( 0.8 ) ) );
        pad->SetAttribute( PAD_ATTRIB::PTH );
        pad->SetLayerSet( LSET::AllCuMask() | LSET( { F_Mask, B_Mask } ) );
        pad->SetNetCode( aNetCode );
        fp->Add( pad );

        m_board->Add( fp );
        return fp;
    }

    /**
     * Set backdrill on a pad
     */
    void SetPadBackdrill( PAD* aPad, PCB_LAYER_ID aStart, PCB_LAYER_ID aEnd, int aSize )
    {
        aPad->SetSecondaryDrillSize( VECTOR2I( aSize, aSize ) );
        aPad->SetSecondaryDrillStartLayer( aStart );
        aPad->SetSecondaryDrillEndLayer( aEnd );
    }

    /**
     * Set post-machining on a pad
     */
    void SetPadPostMachining( PAD* aPad, bool aFront,
                               PAD_DRILL_POST_MACHINING_MODE aMode, int aSize, int aDepth )
    {
        if( aFront )
        {
            aPad->SetFrontPostMachiningMode( aMode );
            aPad->SetFrontPostMachiningSize( aSize );
            aPad->SetFrontPostMachiningDepth( aDepth );
            if( aMode == PAD_DRILL_POST_MACHINING_MODE::COUNTERSINK )
                aPad->SetFrontPostMachiningAngle( 900 );
        }
        else
        {
            aPad->SetBackPostMachiningMode( aMode );
            aPad->SetBackPostMachiningSize( aSize );
            aPad->SetBackPostMachiningDepth( aDepth );
            if( aMode == PAD_DRILL_POST_MACHINING_MODE::COUNTERSINK )
                aPad->SetBackPostMachiningAngle( 900 );
        }
    }

    /**
     * Create a zone on a specific layer
     */
    ZONE* CreateZone( const VECTOR2I& aCorner1, const VECTOR2I& aCorner2,
                      PCB_LAYER_ID aLayer, int aNetCode )
    {
        ZONE* zone = new ZONE( m_board.get() );
        zone->SetLayer( aLayer );
        zone->SetNetCode( aNetCode );

        SHAPE_POLY_SET outline;
        outline.NewOutline();
        outline.Append( aCorner1 );
        outline.Append( VECTOR2I( aCorner2.x, aCorner1.y ) );
        outline.Append( aCorner2 );
        outline.Append( VECTOR2I( aCorner1.x, aCorner2.y ) );
        zone->AddPolygon( outline.COutline( 0 ) );

        m_board->Add( zone );
        return zone;
    }

    void FillZones()
    {
        KI_TEST::FillZones( m_board.get() );
    }

    void RebuildConnectivity()
    {
        m_board->BuildConnectivity();
    }

    /**
     * Run DRC and collect violations of a specific type
     */
    std::vector<DRC_ITEM> RunDRCForErrorCode( int aErrorCode )
    {
        std::vector<DRC_ITEM> violations;
        BOARD_DESIGN_SETTINGS& bds = m_board->GetDesignSettings();

        bds.m_DRCEngine->SetViolationHandler(
                [&]( const std::shared_ptr<DRC_ITEM>& aItem, const VECTOR2I& aPos, int aLayer,
                     const std::function<void( PCB_MARKER* )>& aPathGenerator )
                {
                    if( aItem->GetErrorCode() == aErrorCode )
                        violations.push_back( *aItem );
                } );

        bds.m_DRCEngine->RunTests( EDA_UNITS::MM, true, false );

        return violations;
    }

    int GetNetCode( const wxString& aNetName )
    {
        NETINFO_ITEM* net = m_board->FindNet( aNetName );
        if( !net )
        {
            net = new NETINFO_ITEM( m_board.get(), aNetName );
            m_board->Add( net );
        }
        return net->GetNetCode();
    }

    SETTINGS_MANAGER       m_settingsManager;
    std::unique_ptr<BOARD> m_board;
};


/**
 * Test that IsBackdrilledOrPostMachined correctly identifies affected layers for vias
 */
BOOST_FIXTURE_TEST_CASE( ViaBackdrillLayerDetection, BACKDRILL_TEST_FIXTURE )
{
    int netCode = GetNetCode( "TestNet" );

    // Create a via with backdrill from F_Cu to In2_Cu (removing In1_Cu copper)
    PCB_VIA* via = CreateBackdrilledVia(
            VECTOR2I( pcbIUScale.mmToIU( 10 ), pcbIUScale.mmToIU( 10 ) ),
            netCode,
            F_Cu, B_Cu,          // Primary drill: full through-hole
            F_Cu, In2_Cu,        // Backdrill removes copper on F_Cu, In1_Cu
            pcbIUScale.mmToIU( 0.5 ) );

    // F_Cu should be affected (within backdrill range)
    BOOST_CHECK( via->IsBackdrilledOrPostMachined( F_Cu ) );

    // In1_Cu should be affected (within backdrill range)
    BOOST_CHECK( via->IsBackdrilledOrPostMachined( In1_Cu ) );

    // In2_Cu is the end layer - behavior depends on implementation
    // In3_Cu should NOT be affected (beyond backdrill end)
    BOOST_CHECK( !via->IsBackdrilledOrPostMachined( In3_Cu ) );

    // B_Cu should NOT be affected
    BOOST_CHECK( !via->IsBackdrilledOrPostMachined( B_Cu ) );
}


/**
 * Regression test for GitLab #23902.  Iterating LAYER_RANGE walked PCB_LAYER_ID enum order, so a
 * bottom-anchored backdrill (B_Cu to In3_Cu) wrongly flagged the top inner layers and skipped the
 * actually-drilled In4_Cu.  A bottom backdrill must report only the bottom-side copper as machined.
 */
BOOST_FIXTURE_TEST_CASE( ViaBottomBackdrillLayerDetection, BACKDRILL_TEST_FIXTURE )
{
    int netCode = GetNetCode( "TestNet" );

    // 6-layer stackup top to bottom is F_Cu, In1_Cu, In2_Cu, In3_Cu, In4_Cu, B_Cu.  Back-drilling
    // from the bottom (B_Cu) up to In3_Cu removes copper on In3_Cu, In4_Cu and B_Cu only.
    PCB_VIA* via = CreateBackdrilledVia(
            VECTOR2I( pcbIUScale.mmToIU( 10 ), pcbIUScale.mmToIU( 30 ) ),
            netCode,
            F_Cu, B_Cu,          // Primary drill: full through-hole
            B_Cu, In3_Cu,        // Backdrill from the bottom up to In3_Cu
            pcbIUScale.mmToIU( 0.5 ) );

    // Top-side layers must NOT be reported as backdrilled.
    BOOST_CHECK( !via->IsBackdrilledOrPostMachined( F_Cu ) );
    BOOST_CHECK( !via->IsBackdrilledOrPostMachined( In1_Cu ) );
    BOOST_CHECK( !via->IsBackdrilledOrPostMachined( In2_Cu ) );

    // The drilled span from In3_Cu down to B_Cu must be reported as backdrilled.
    BOOST_CHECK( via->IsBackdrilledOrPostMachined( In3_Cu ) );
    BOOST_CHECK( via->IsBackdrilledOrPostMachined( In4_Cu ) );
    BOOST_CHECK( via->IsBackdrilledOrPostMachined( B_Cu ) );
}


/**
 * Test that IsBackdrilledOrPostMachined correctly identifies affected layers for post-machining
 */
BOOST_FIXTURE_TEST_CASE( ViaPostMachiningLayerDetection, BACKDRILL_TEST_FIXTURE )
{
    int netCode = GetNetCode( "TestNet" );

    // Create a via with front countersink post-machining
    // Post-machining depth determines which layers are affected
    PCB_VIA* via = CreatePostMachinedVia(
            VECTOR2I( pcbIUScale.mmToIU( 20 ), pcbIUScale.mmToIU( 10 ) ),
            netCode,
            PAD_DRILL_POST_MACHINING_MODE::COUNTERSINK,
            pcbIUScale.mmToIU( 1.0 ),    // Size
            pcbIUScale.mmToIU( 0.5 ) );  // Depth - should affect F_Cu and potentially In1_Cu

    // F_Cu should be affected (front post-machining starts there)
    BOOST_CHECK( via->IsBackdrilledOrPostMachined( F_Cu ) );

    // B_Cu should NOT be affected (no back post-machining)
    BOOST_CHECK( !via->IsBackdrilledOrPostMachined( B_Cu ) );
}


/**
 * Test that IsBackdrilledOrPostMachined correctly identifies affected layers for pads
 */
BOOST_FIXTURE_TEST_CASE( PadBackdrillLayerDetection, BACKDRILL_TEST_FIXTURE )
{
    int netCode = GetNetCode( "TestNet" );

    FOOTPRINT* fp = CreateFootprintWithPad(
            VECTOR2I( pcbIUScale.mmToIU( 30 ), pcbIUScale.mmToIU( 10 ) ),
            netCode );

    PAD* pad = fp->Pads().front();

    // Set backdrill on the pad from F_Cu to In2_Cu
    SetPadBackdrill( pad, F_Cu, In2_Cu, pcbIUScale.mmToIU( 1.0 ) );

    // F_Cu should be affected
    BOOST_CHECK( pad->IsBackdrilledOrPostMachined( F_Cu ) );

    // In1_Cu should be affected
    BOOST_CHECK( pad->IsBackdrilledOrPostMachined( In1_Cu ) );

    // In3_Cu should NOT be affected
    BOOST_CHECK( !pad->IsBackdrilledOrPostMachined( In3_Cu ) );

    // B_Cu should NOT be affected
    BOOST_CHECK( !pad->IsBackdrilledOrPostMachined( B_Cu ) );
}


/**
 * Test that GetEffectiveShape returns the backdrill hole shape for affected layers
 */
BOOST_FIXTURE_TEST_CASE( ViaEffectiveShapeOnBackdrilledLayer, BACKDRILL_TEST_FIXTURE )
{
    int netCode = GetNetCode( "TestNet" );

    int backdillSize = pcbIUScale.mmToIU( 0.6 );
    int viaWidth = pcbIUScale.mmToIU( 0.8 );

    PCB_VIA* via = CreateBackdrilledVia(
            VECTOR2I( pcbIUScale.mmToIU( 40 ), pcbIUScale.mmToIU( 10 ) ),
            netCode,
            F_Cu, B_Cu,
            F_Cu, In2_Cu,
            backdillSize );

    via->SetWidth( PADSTACK::ALL_LAYERS, viaWidth );

    // On a non-affected layer, should return full via size
    std::shared_ptr<SHAPE> shapeB = via->GetEffectiveShape( B_Cu );
    BOOST_REQUIRE( shapeB );

    // On an affected layer, should return backdrill hole size
    std::shared_ptr<SHAPE> shapeF = via->GetEffectiveShape( F_Cu );
    BOOST_REQUIRE( shapeF );

    // The effective shape on the backdrilled layer should be smaller (hole only)
    BOX2I bboxB = shapeB->BBox();
    BOX2I bboxF = shapeF->BBox();

    // Shape on B_Cu should be full via size
    BOOST_CHECK_GE( bboxB.GetWidth(), viaWidth - 100 ); // Allow small tolerance

    // Shape on F_Cu should be backdrill size (smaller than via)
    BOOST_CHECK_LE( bboxF.GetWidth(), backdillSize + 100 );
}


/**
 * Test that connectivity correctly excludes backdrilled layers for zones
 */
BOOST_FIXTURE_TEST_CASE( ZoneConnectivityWithBackdrill, BACKDRILL_TEST_FIXTURE )
{
    int netCode = GetNetCode( "TestNet" );

    // Create a via with backdrill
    PCB_VIA* via = CreateBackdrilledVia(
            VECTOR2I( pcbIUScale.mmToIU( 50 ), pcbIUScale.mmToIU( 50 ) ),
            netCode,
            F_Cu, B_Cu,
            F_Cu, In2_Cu,  // Backdrill removes F_Cu and In1_Cu
            pcbIUScale.mmToIU( 0.5 ) );

    // Create a zone on F_Cu (backdrilled layer) with same net
    ZONE* zone = CreateZone(
            VECTOR2I( pcbIUScale.mmToIU( 40 ), pcbIUScale.mmToIU( 40 ) ),
            VECTOR2I( pcbIUScale.mmToIU( 60 ), pcbIUScale.mmToIU( 60 ) ),
            F_Cu, netCode );

    FillZones();
    RebuildConnectivity();

    // The via should NOT be connected to the zone on F_Cu because it's backdrilled
    // This tests the connectivity algorithm update
    auto connectivity = m_board->GetConnectivity();

    // Get items connected to the via
    std::vector<BOARD_CONNECTED_ITEM*> connectedItems = connectivity->GetConnectedItems( via, 0 );

    // Check if zone is in connected items
    bool zoneConnected = false;
    for( BOARD_CONNECTED_ITEM* item : connectedItems )
    {
        if( item == zone )
            zoneConnected = true;
    }

    // The via is backdrilled on F_Cu, so its copper there is removed and it must NOT be
    // reported as connected to the F_Cu zone on the same net.
    BOOST_CHECK_MESSAGE( !zoneConnected,
                         "Backdrilled via must not connect to the zone on the backdrilled layer" );
}


/**
 * Test DRC error for track connected to post-machined layer
 */
BOOST_FIXTURE_TEST_CASE( DRCTrackOnPostMachinedLayer, BACKDRILL_TEST_FIXTURE )
{
    int netCode = GetNetCode( "TestNet" );

    // Create a via with post-machining on F_Cu
    PCB_VIA* via = CreatePostMachinedVia(
            VECTOR2I( pcbIUScale.mmToIU( 60 ), pcbIUScale.mmToIU( 10 ) ),
            netCode,
            PAD_DRILL_POST_MACHINING_MODE::COUNTERBORE,
            pcbIUScale.mmToIU( 1.2 ),
            pcbIUScale.mmToIU( 0.3 ) );

    // Create a track on F_Cu connected to the via (this should trigger DRC error)
    PCB_TRACK* track = CreateTrack(
            via->GetPosition(),
            VECTOR2I( pcbIUScale.mmToIU( 70 ), pcbIUScale.mmToIU( 10 ) ),
            F_Cu, netCode );

    RebuildConnectivity();

    // Run DRC and check for DRCE_TRACK_ON_POST_MACHINED_LAYER
    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_TRACK_ON_POST_MACHINED_LAYER );

    BOOST_CHECK_GE( violations.size(), 1u );
}


/**
 * Test DRC error for track connected to backdrilled layer
 */
BOOST_FIXTURE_TEST_CASE( DRCTrackOnBackdrilledLayer, BACKDRILL_TEST_FIXTURE )
{
    int netCode = GetNetCode( "TestNet" );

    // Create a via with backdrill removing In1_Cu
    PCB_VIA* via = CreateBackdrilledVia(
            VECTOR2I( pcbIUScale.mmToIU( 70 ), pcbIUScale.mmToIU( 10 ) ),
            netCode,
            F_Cu, B_Cu,
            F_Cu, In2_Cu,  // Backdrill affects F_Cu, In1_Cu
            pcbIUScale.mmToIU( 0.5 ) );

    // Create a track on In1_Cu connected to the via (this should trigger DRC error)
    PCB_TRACK* track = CreateTrack(
            via->GetPosition(),
            VECTOR2I( pcbIUScale.mmToIU( 80 ), pcbIUScale.mmToIU( 10 ) ),
            In1_Cu, netCode );

    RebuildConnectivity();

    // Run DRC and check for DRCE_TRACK_ON_POST_MACHINED_LAYER
    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_TRACK_ON_POST_MACHINED_LAYER );

    BOOST_CHECK_GE( violations.size(), 1u );
}


/**
 * Test that tracks on non-affected layers don't trigger DRC errors
 */
BOOST_FIXTURE_TEST_CASE( DRCTrackOnUnaffectedLayerNoDRC, BACKDRILL_TEST_FIXTURE )
{
    int netCode = GetNetCode( "TestNet" );

    // Create a via with backdrill removing F_Cu and In1_Cu
    PCB_VIA* via = CreateBackdrilledVia(
            VECTOR2I( pcbIUScale.mmToIU( 80 ), pcbIUScale.mmToIU( 10 ) ),
            netCode,
            F_Cu, B_Cu,
            F_Cu, In2_Cu,
            pcbIUScale.mmToIU( 0.5 ) );

    // Create a track on B_Cu (not affected by backdrill) - should NOT trigger error
    PCB_TRACK* track = CreateTrack(
            via->GetPosition(),
            VECTOR2I( pcbIUScale.mmToIU( 90 ), pcbIUScale.mmToIU( 10 ) ),
            B_Cu, netCode );

    RebuildConnectivity();

    // Run DRC - should NOT find violations for DRCE_TRACK_ON_POST_MACHINED_LAYER
    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_TRACK_ON_POST_MACHINED_LAYER );

    // Filter to only violations involving our track
    int trackViolations = 0;
    for( const DRC_ITEM& item : violations )
    {
        if( item.GetMainItemID() == track->m_Uuid || item.GetAuxItemID() == track->m_Uuid )
            trackViolations++;
    }

    BOOST_CHECK_EQUAL( trackViolations, 0 );
}


/**
 * Test DRC for pad with backdrill and connected track
 */
BOOST_FIXTURE_TEST_CASE( DRCTrackOnBackdrilledPadLayer, BACKDRILL_TEST_FIXTURE )
{
    int netCode = GetNetCode( "TestNet" );

    FOOTPRINT* fp = CreateFootprintWithPad(
            VECTOR2I( pcbIUScale.mmToIU( 90 ), pcbIUScale.mmToIU( 10 ) ),
            netCode );

    PAD* pad = fp->Pads().front();

    // Set backdrill on the pad from F_Cu to In2_Cu
    SetPadBackdrill( pad, F_Cu, In2_Cu, pcbIUScale.mmToIU( 1.0 ) );

    // Create a track on In1_Cu connected to the pad (should trigger DRC)
    PCB_TRACK* track = CreateTrack(
            pad->GetPosition(),
            VECTOR2I( pcbIUScale.mmToIU( 100 ), pcbIUScale.mmToIU( 10 ) ),
            In1_Cu, netCode );

    RebuildConnectivity();

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_TRACK_ON_POST_MACHINED_LAYER );

    BOOST_CHECK_GE( violations.size(), 1u );
}


/**
 * Test that pad post-machining is correctly detected
 */
BOOST_FIXTURE_TEST_CASE( PadPostMachiningLayerDetection, BACKDRILL_TEST_FIXTURE )
{
    int netCode = GetNetCode( "TestNet" );

    FOOTPRINT* fp = CreateFootprintWithPad(
            VECTOR2I( pcbIUScale.mmToIU( 100 ), pcbIUScale.mmToIU( 10 ) ),
            netCode );

    PAD* pad = fp->Pads().front();

    // Set front post-machining (counterbore)
    SetPadPostMachining( pad, true,
                          PAD_DRILL_POST_MACHINING_MODE::COUNTERBORE,
                          pcbIUScale.mmToIU( 1.5 ),
                          pcbIUScale.mmToIU( 0.4 ) );

    // F_Cu should be affected by front post-machining
    BOOST_CHECK( pad->IsBackdrilledOrPostMachined( F_Cu ) );

    // B_Cu should NOT be affected
    BOOST_CHECK( !pad->IsBackdrilledOrPostMachined( B_Cu ) );
}


/**
 * Test back post-machining detection
 */
BOOST_FIXTURE_TEST_CASE( PadBackPostMachiningLayerDetection, BACKDRILL_TEST_FIXTURE )
{
    int netCode = GetNetCode( "TestNet" );

    FOOTPRINT* fp = CreateFootprintWithPad(
            VECTOR2I( pcbIUScale.mmToIU( 110 ), pcbIUScale.mmToIU( 10 ) ),
            netCode );

    PAD* pad = fp->Pads().front();

    // Set back post-machining (countersink)
    SetPadPostMachining( pad, false,
                          PAD_DRILL_POST_MACHINING_MODE::COUNTERSINK,
                          pcbIUScale.mmToIU( 1.5 ),
                          pcbIUScale.mmToIU( 0.4 ) );

    // B_Cu should be affected by back post-machining
    BOOST_CHECK( pad->IsBackdrilledOrPostMachined( B_Cu ) );

    // F_Cu should NOT be affected
    BOOST_CHECK( !pad->IsBackdrilledOrPostMachined( F_Cu ) );
}


/**
 * Combined test: both backdrill and post-machining on same via
 */
BOOST_FIXTURE_TEST_CASE( ViaBothBackdrillAndPostMachining, BACKDRILL_TEST_FIXTURE )
{
    int netCode = GetNetCode( "TestNet" );

    PCB_VIA* via = new PCB_VIA( m_board.get() );
    via->SetPosition( VECTOR2I( pcbIUScale.mmToIU( 120 ), pcbIUScale.mmToIU( 10 ) ) );
    via->SetLayerPair( F_Cu, B_Cu );
    via->SetDrill( pcbIUScale.mmToIU( 0.3 ) );
    via->SetWidth( PADSTACK::ALL_LAYERS, pcbIUScale.mmToIU( 0.6 ) );
    via->SetNetCode( netCode );

    // Set backdrill from back side (B_Cu to In2_Cu)
    // On a 6-layer board: F_Cu, In1_Cu, In2_Cu, In3_Cu, B_Cu
    // Backdrill from B_Cu toward In2_Cu affects B_Cu, In3_Cu (layers in the drill path)
    via->SetSecondaryDrillSize( pcbIUScale.mmToIU( 0.5 ) );
    via->SetSecondaryDrillStartLayer( B_Cu );
    via->SetSecondaryDrillEndLayer( In2_Cu );

    // Set front post-machining
    via->SetFrontPostMachiningMode( PAD_DRILL_POST_MACHINING_MODE::COUNTERBORE );
    via->SetFrontPostMachiningSize( pcbIUScale.mmToIU( 1.0 ) );
    via->SetFrontPostMachiningDepth( pcbIUScale.mmToIU( 0.2 ) );

    m_board->Add( via );

    // F_Cu affected by post-machining
    BOOST_CHECK( via->IsBackdrilledOrPostMachined( F_Cu ) );

    // B_Cu affected by backdrill (start layer)
    BOOST_CHECK( via->IsBackdrilledOrPostMachined( B_Cu ) );

    // In3_Cu lies within the B_Cu -> In2_Cu backdrill path and must be detected as affected.
    BOOST_CHECK( via->IsBackdrilledOrPostMachined( In3_Cu ) );

    // In1_Cu is outside both the (front) post-machining and the (back-side) backdrill span,
    // so it must be unaffected. This relies on the front post-machining depth (0.2mm) staying
    // shallower than the F_Cu -> In1_Cu spacing: the fixture's default 6-layer stackup gives
    // ~0.27mm dielectric per layer (1.6mm board, 6 x 0.035mm copper, 5 dielectrics), a
    // comfortable ~0.09mm margin above the 0.2mm depth.
    BOOST_CHECK( !via->IsBackdrilledOrPostMachined( In1_Cu ) );
}


/**
 * Verify that countersink angle is stored in decidegrees and the property system
 * round-trips correctly. Regression test for GitLab #23134.
 */
BOOST_FIXTURE_TEST_CASE( CountersinkAngleDecidegrees, BACKDRILL_TEST_FIXTURE )
{
    int netCode = GetNetCode( "TestNet" );

    PCB_VIA* via = new PCB_VIA( m_board.get() );
    via->SetPosition( VECTOR2I( pcbIUScale.mmToIU( 130 ), pcbIUScale.mmToIU( 10 ) ) );
    via->SetLayerPair( F_Cu, B_Cu );
    via->SetDrill( pcbIUScale.mmToIU( 0.3 ) );
    via->SetWidth( PADSTACK::ALL_LAYERS, pcbIUScale.mmToIU( 0.6 ) );
    via->SetNetCode( netCode );

    via->SetFrontPostMachiningMode( PAD_DRILL_POST_MACHINING_MODE::COUNTERSINK );
    via->SetFrontPostMachiningSize( pcbIUScale.mmToIU( 1.0 ) );

    // 45 degrees = 450 decidegrees
    via->SetFrontPostMachiningAngle( 450 );
    BOOST_CHECK_EQUAL( via->GetFrontPostMachiningAngle(), 450 );

    // 82 degrees = 820 decidegrees
    via->SetFrontPostMachiningAngle( 820 );
    BOOST_CHECK_EQUAL( via->GetFrontPostMachiningAngle(), 820 );

    // 90 degrees = 900 decidegrees
    via->SetFrontPostMachiningAngle( 900 );
    BOOST_CHECK_EQUAL( via->GetFrontPostMachiningAngle(), 900 );

    via->SetBackPostMachiningMode( PAD_DRILL_POST_MACHINING_MODE::COUNTERSINK );
    via->SetBackPostMachiningSize( pcbIUScale.mmToIU( 1.2 ) );
    via->SetBackPostMachiningAngle( 600 );  // 60 degrees
    BOOST_CHECK_EQUAL( via->GetBackPostMachiningAngle(), 600 );

    m_board->Add( via );

    // Verify the internal storage matches the file format expectation.
    // The file format stores degrees (angle / 10.0), so 450 decideg -> 45.0 deg in file.
    const PADSTACK::POST_MACHINING_PROPS& frontPM = via->Padstack().FrontPostMachining();
    BOOST_CHECK_EQUAL( frontPM.angle, 900 );

    const PADSTACK::POST_MACHINING_PROPS& backPM = via->Padstack().BackPostMachining();
    BOOST_CHECK_EQUAL( backPM.angle, 600 );
}


/**
 * A backdrill whose diameter is smaller than the primary via drill can never remove the barrel
 * and must be flagged as an invalid span.
 */
BOOST_FIXTURE_TEST_CASE( DRCBackdrillInvalidDiameter, BACKDRILL_TEST_FIXTURE )
{
    int netCode = GetNetCode( "TestNet" );

    // Primary drill is 0.3mm; a 0.2mm backdrill is smaller than the hole it should clear.
    CreateBackdrilledVia( VECTOR2I( pcbIUScale.mmToIU( 20 ), pcbIUScale.mmToIU( 20 ) ), netCode,
                          F_Cu, B_Cu, F_Cu, In3_Cu, pcbIUScale.mmToIU( 0.2 ) );

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_BACKDRILL_INVALID_SPAN );
    BOOST_CHECK_GE( violations.size(), 1u );
}


/**
 * A backdrill must enter from an outer copper layer; one whose start layer is an inner layer is
 * not manufacturable and must be flagged.
 */
BOOST_FIXTURE_TEST_CASE( DRCBackdrillStartNotOuterLayer, BACKDRILL_TEST_FIXTURE )
{
    int netCode = GetNetCode( "TestNet" );

    CreateBackdrilledVia( VECTOR2I( pcbIUScale.mmToIU( 30 ), pcbIUScale.mmToIU( 30 ) ), netCode,
                          F_Cu, B_Cu, In2_Cu, In3_Cu, pcbIUScale.mmToIU( 0.5 ) );

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_BACKDRILL_INVALID_SPAN );
    BOOST_CHECK_GE( violations.size(), 1u );
}


/**
 * A well-formed backdrill (enters at F_Cu, must-cut at an inner copper layer, diameter larger
 * than the primary drill) must not raise an invalid-span violation.
 */
BOOST_FIXTURE_TEST_CASE( DRCBackdrillValidNoViolation, BACKDRILL_TEST_FIXTURE )
{
    int netCode = GetNetCode( "TestNet" );

    CreateBackdrilledVia( VECTOR2I( pcbIUScale.mmToIU( 40 ), pcbIUScale.mmToIU( 40 ) ), netCode,
                          F_Cu, B_Cu, F_Cu, In3_Cu, pcbIUScale.mmToIU( 0.5 ) );

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_BACKDRILL_INVALID_SPAN );
    BOOST_CHECK_EQUAL( violations.size(), 0u );
}


namespace
{
// Write a .kicad_dru with a single backdrill_stub_length max rule (into a per-tag temp dir) and
// point the fixture's DRC engine at it. Returns the temp dir so the caller can remove it.
std::filesystem::path WriteBackdrillStubRule( BOARD* aBoard, const wxString& aTag,
                                              const wxString& aMaxLength )
{
    namespace fs = std::filesystem;
    fs::path tmpDir = fs::temp_directory_path()
                      / ( "kicad_drc_backdrill_stub_" + aTag.ToStdString() );
    fs::create_directories( tmpDir );
    fs::path druPath = tmpDir / "backdrill_stub.kicad_dru";

    {
        std::ofstream out( druPath );
        out << "(version 1)\n"
            << "(rule \"BackdrillStub\"\n"
            << "    (constraint backdrill_stub_length (max " << aMaxLength.ToStdString() << "))\n"
            << ")\n";
    }

    aBoard->GetDesignSettings().m_DRCEngine->InitEngine( wxFileName( druPath.string() ) );
    return tmpDir;
}
} // namespace


/**
 * A via whose signal exits deep (In4_Cu) but whose top backdrill only reaches In1_Cu leaves a
 * long residual barrel (In1_Cu -> In4_Cu) that must violate a tight stub budget.
 */
BOOST_FIXTURE_TEST_CASE( DRCBackdrillStubTooLong, BACKDRILL_TEST_FIXTURE )
{
    int      netCode = GetNetCode( "TestNet" );
    VECTOR2I pos( pcbIUScale.mmToIU( 50 ), pcbIUScale.mmToIU( 50 ) );

    CreateBackdrilledVia( pos, netCode, F_Cu, B_Cu, F_Cu, In1_Cu, pcbIUScale.mmToIU( 0.5 ) );
    CreateTrack( pos, VECTOR2I( pcbIUScale.mmToIU( 60 ), pcbIUScale.mmToIU( 50 ) ), In4_Cu, netCode );
    RebuildConnectivity();

    std::filesystem::path dir =
            WriteBackdrillStubRule( m_board.get(), wxT( "toolong" ), wxT( "0.2mm" ) );

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_BACKDRILL_STUB_TOO_LONG );
    BOOST_CHECK_GE( violations.size(), 1u );

    std::filesystem::remove_all( dir );
}


/**
 * A backdrill that reaches In3_Cu, just above the In4_Cu signal exit, leaves only a short
 * residual and must not violate a generous stub budget.
 */
BOOST_FIXTURE_TEST_CASE( DRCBackdrillStubWithinBudget, BACKDRILL_TEST_FIXTURE )
{
    int      netCode = GetNetCode( "TestNet" );
    VECTOR2I pos( pcbIUScale.mmToIU( 60 ), pcbIUScale.mmToIU( 60 ) );

    CreateBackdrilledVia( pos, netCode, F_Cu, B_Cu, F_Cu, In3_Cu, pcbIUScale.mmToIU( 0.5 ) );
    CreateTrack( pos, VECTOR2I( pcbIUScale.mmToIU( 70 ), pcbIUScale.mmToIU( 60 ) ), In4_Cu, netCode );
    RebuildConnectivity();

    std::filesystem::path dir =
            WriteBackdrillStubRule( m_board.get(), wxT( "budget" ), wxT( "1mm" ) );

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_BACKDRILL_STUB_TOO_LONG );
    BOOST_CHECK_EQUAL( violations.size(), 0u );

    std::filesystem::remove_all( dir );
}


/**
 * The residual stub is measured against the via's actual connections, not the board's outer
 * layers. A via that connects only shallow (In1_Cu), with no same-net copper beyond the In2_Cu
 * must-cut layer, has no stub - so even a very tight budget must not fire. (The naive
 * "must-cut -> opposite outer layer" definition would false-positive here.)
 */
BOOST_FIXTURE_TEST_CASE( DRCBackdrillStubNoConnectionBeyondMustCut, BACKDRILL_TEST_FIXTURE )
{
    int      netCode = GetNetCode( "TestNet" );
    VECTOR2I pos( pcbIUScale.mmToIU( 70 ), pcbIUScale.mmToIU( 70 ) );

    CreateBackdrilledVia( pos, netCode, F_Cu, B_Cu, F_Cu, In2_Cu, pcbIUScale.mmToIU( 0.5 ) );
    CreateTrack( pos, VECTOR2I( pcbIUScale.mmToIU( 80 ), pcbIUScale.mmToIU( 70 ) ), In1_Cu, netCode );
    RebuildConnectivity();

    std::filesystem::path dir =
            WriteBackdrillStubRule( m_board.get(), wxT( "noconn" ), wxT( "0.1mm" ) );

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_BACKDRILL_STUB_TOO_LONG );
    BOOST_CHECK_EQUAL( violations.size(), 0u );

    std::filesystem::remove_all( dir );
}


/**
 * The residual must be derived from the via's own routing landings, not the whole net cluster.
 * A backdrilled via routed only shallow (In1_Cu) but sharing a net with a plain full-span through
 * via must report no stub: a whole-cluster query would wrongly treat the other via's full-copper
 * span as connections beyond the must-cut and false-positive under a tight budget.
 */
BOOST_FIXTURE_TEST_CASE( DRCBackdrillStubMultiConductorNet, BACKDRILL_TEST_FIXTURE )
{
    int      netCode = GetNetCode( "TestNet" );
    VECTOR2I posA( pcbIUScale.mmToIU( 50 ), pcbIUScale.mmToIU( 50 ) );
    VECTOR2I posB( pcbIUScale.mmToIU( 70 ), pcbIUScale.mmToIU( 50 ) );

    // Backdrilled via A, routed only shallow (In1_Cu) -> no signal beyond the In2_Cu must-cut.
    CreateBackdrilledVia( posA, netCode, F_Cu, B_Cu, F_Cu, In2_Cu, pcbIUScale.mmToIU( 0.5 ) );

    // A plain full-span through via B on the same net.
    PCB_VIA* viaB = new PCB_VIA( m_board.get() );
    viaB->SetPosition( posB );
    viaB->SetLayerPair( F_Cu, B_Cu );
    viaB->SetDrill( pcbIUScale.mmToIU( 0.3 ) );
    viaB->SetWidth( PADSTACK::ALL_LAYERS, pcbIUScale.mmToIU( 0.6 ) );
    viaB->SetNetCode( netCode );
    m_board->Add( viaB );

    // Connect A and B (same cluster) with a shallow In1_Cu track.
    CreateTrack( posA, posB, In1_Cu, netCode );
    RebuildConnectivity();

    std::filesystem::path dir =
            WriteBackdrillStubRule( m_board.get(), wxT( "multi" ), wxT( "0.1mm" ) );

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_BACKDRILL_STUB_TOO_LONG );
    BOOST_CHECK_EQUAL( violations.size(), 0u );

    std::filesystem::remove_all( dir );
}


/**
 * Two backdrilled vias whose primary drills clear the hole-to-hole minimum but whose enlarged
 * backdrill bores do not must be flagged: the hole-to-hole check now models the via hole at the
 * largest drilled diameter.
 */
BOOST_FIXTURE_TEST_CASE( DRCBackdrillHoleToHoleTooClose, BACKDRILL_TEST_FIXTURE )
{
    BOARD_DESIGN_SETTINGS& bds = m_board->GetDesignSettings();
    bds.m_HoleToHoleMin = pcbIUScale.mmToIU( 0.3 );
    bds.m_DRCEngine->InitEngine( wxFileName() ); // rebuild implicit board-setup rules

    int netCode = GetNetCode( "TestNet" );

    // Centres 0.7mm apart: 0.3mm primary drills are 0.4mm apart (clear) but the 0.5mm backdrill
    // bores are only 0.2mm apart, under the 0.3mm hole-to-hole minimum.
    CreateBackdrilledVia( VECTOR2I( 0, 0 ), netCode, F_Cu, B_Cu, F_Cu, In3_Cu,
                          pcbIUScale.mmToIU( 0.5 ) );
    CreateBackdrilledVia( VECTOR2I( pcbIUScale.mmToIU( 0.7 ), 0 ), netCode, F_Cu, B_Cu, F_Cu, In3_Cu,
                          pcbIUScale.mmToIU( 0.5 ) );

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_DRILLED_HOLES_TOO_CLOSE );
    BOOST_CHECK_GE( violations.size(), 1u );
}


/**
 * The same two backdrilled vias, spaced so even the backdrill bores clear the minimum, must not
 * be flagged.
 */
BOOST_FIXTURE_TEST_CASE( DRCBackdrillHoleToHoleClear, BACKDRILL_TEST_FIXTURE )
{
    BOARD_DESIGN_SETTINGS& bds = m_board->GetDesignSettings();
    bds.m_HoleToHoleMin = pcbIUScale.mmToIU( 0.3 );
    bds.m_DRCEngine->InitEngine( wxFileName() );

    int netCode = GetNetCode( "TestNet" );

    // Centres 1.2mm apart: the 0.5mm backdrill bores are 0.7mm apart, well over the 0.3mm minimum.
    CreateBackdrilledVia( VECTOR2I( 0, 0 ), netCode, F_Cu, B_Cu, F_Cu, In3_Cu,
                          pcbIUScale.mmToIU( 0.5 ) );
    CreateBackdrilledVia( VECTOR2I( pcbIUScale.mmToIU( 1.2 ), 0 ), netCode, F_Cu, B_Cu, F_Cu, In3_Cu,
                          pcbIUScale.mmToIU( 0.5 ) );

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_DRILLED_HOLES_TOO_CLOSE );
    BOOST_CHECK_EQUAL( violations.size(), 0u );
}


/**
 * A counterbore/countersink post-machining enlarges the drilled bore just like a backdrill, so
 * hole-to-hole clearance must account for it too.  Two vias whose 0.3mm primary drills clear the
 * minimum but whose 0.5mm front counterbores do not must be flagged.  (Under the backdrill-only
 * model the primary drills are 0.4mm apart and this raised no violation.)
 */
BOOST_FIXTURE_TEST_CASE( DRCPostMachiningHoleToHoleTooClose, BACKDRILL_TEST_FIXTURE )
{
    BOARD_DESIGN_SETTINGS& bds = m_board->GetDesignSettings();
    bds.m_HoleToHoleMin = pcbIUScale.mmToIU( 0.3 );
    bds.m_DRCEngine->InitEngine( wxFileName() ); // rebuild implicit board-setup rules

    int netCode = GetNetCode( "TestNet" );

    // Centres 0.7mm apart: 0.3mm primary drills are 0.4mm apart (clear) but the 0.5mm front
    // counterbores are only 0.2mm apart, under the 0.3mm hole-to-hole minimum.
    CreatePostMachinedVia( VECTOR2I( 0, 0 ), netCode,
                           PAD_DRILL_POST_MACHINING_MODE::COUNTERBORE,
                           pcbIUScale.mmToIU( 0.5 ), pcbIUScale.mmToIU( 0.2 ) );
    CreatePostMachinedVia( VECTOR2I( pcbIUScale.mmToIU( 0.7 ), 0 ), netCode,
                           PAD_DRILL_POST_MACHINING_MODE::COUNTERBORE,
                           pcbIUScale.mmToIU( 0.5 ), pcbIUScale.mmToIU( 0.2 ) );

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_DRILLED_HOLES_TOO_CLOSE );
    BOOST_CHECK_GE( violations.size(), 1u );
}


/**
 * The same two post-machined vias, spaced so even the counterbores clear the minimum, must not be
 * flagged.
 */
BOOST_FIXTURE_TEST_CASE( DRCPostMachiningHoleToHoleClear, BACKDRILL_TEST_FIXTURE )
{
    BOARD_DESIGN_SETTINGS& bds = m_board->GetDesignSettings();
    bds.m_HoleToHoleMin = pcbIUScale.mmToIU( 0.3 );
    bds.m_DRCEngine->InitEngine( wxFileName() );

    int netCode = GetNetCode( "TestNet" );

    // Centres 1.2mm apart: the 0.5mm counterbores are 0.7mm apart, well over the 0.3mm minimum.
    CreatePostMachinedVia( VECTOR2I( 0, 0 ), netCode,
                           PAD_DRILL_POST_MACHINING_MODE::COUNTERBORE,
                           pcbIUScale.mmToIU( 0.5 ), pcbIUScale.mmToIU( 0.2 ) );
    CreatePostMachinedVia( VECTOR2I( pcbIUScale.mmToIU( 1.2 ), 0 ), netCode,
                           PAD_DRILL_POST_MACHINING_MODE::COUNTERBORE,
                           pcbIUScale.mmToIU( 0.5 ), pcbIUScale.mmToIU( 0.2 ) );

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_DRILLED_HOLES_TOO_CLOSE );
    BOOST_CHECK_EQUAL( violations.size(), 0u );
}


/**
 * Hole-to-copper clearance must account for the enlarged backdrill bore.  A different-net track on
 * a backdrilled layer, clear of the 0.3mm primary drill but inside the 0.8mm backdrill bore's
 * clearance, must raise a hole-clearance violation.  The track sits far enough out that the via is
 * only reached once the copper r-tree search margin is widened by the bore enlargement AND the
 * hole is modelled at the bore, so this exercises both halves of the fix.
 */
BOOST_FIXTURE_TEST_CASE( DRCBackdrillHoleToCopperTooClose, BACKDRILL_TEST_FIXTURE )
{
    BOARD_DESIGN_SETTINGS& bds = m_board->GetDesignSettings();
    bds.m_HoleClearance = pcbIUScale.mmToIU( 0.25 );
    bds.m_MinClearance = pcbIUScale.mmToIU( 0.1 );
    bds.m_DRCEngine->InitEngine( wxFileName() );

    int viaNet = GetNetCode( "ViaNet" );
    int trackNet = GetNetCode( "TrackNet" );

    VECTOR2I viaPos( pcbIUScale.mmToIU( 10 ), pcbIUScale.mmToIU( 10 ) );

    // 0.3mm primary drill, 0.6mm copper, 0.8mm backdrill F_Cu -> In3_Cu.
    CreateBackdrilledVia( viaPos, viaNet, F_Cu, B_Cu, F_Cu, In3_Cu, pcbIUScale.mmToIU( 0.8 ) );

    // Track (0.25mm wide) on the backdrilled F_Cu, 0.7mm from the via centre: 0.175mm from the
    // 0.4mm bore radius (< 0.25mm -> violation) but 0.425mm from the 0.15mm primary radius.
    int x = pcbIUScale.mmToIU( 10.7 );
    CreateTrack( VECTOR2I( x, pcbIUScale.mmToIU( 9 ) ), VECTOR2I( x, pcbIUScale.mmToIU( 11 ) ),
                 F_Cu, trackNet );
    RebuildConnectivity();

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_HOLE_CLEARANCE );
    BOOST_CHECK_GE( violations.size(), 1u );
}


/**
 * The same via and track, but the track is on B_Cu - below the In3_Cu must-cut, so the backdrill
 * bore never reaches it and only the 0.3mm primary drill is present there.  0.6mm out, it clears
 * the primary drill and must NOT be flagged.  This pins the layer-awareness: a naive "largest bore
 * on every layer" model would false-positive here.
 */
BOOST_FIXTURE_TEST_CASE( DRCBackdrillHoleToCopperUnaffectedLayer, BACKDRILL_TEST_FIXTURE )
{
    BOARD_DESIGN_SETTINGS& bds = m_board->GetDesignSettings();
    bds.m_HoleClearance = pcbIUScale.mmToIU( 0.25 );
    bds.m_MinClearance = pcbIUScale.mmToIU( 0.1 );
    bds.m_DRCEngine->InitEngine( wxFileName() );

    int viaNet = GetNetCode( "ViaNet" );
    int trackNet = GetNetCode( "TrackNet" );

    VECTOR2I viaPos( pcbIUScale.mmToIU( 10 ), pcbIUScale.mmToIU( 10 ) );
    CreateBackdrilledVia( viaPos, viaNet, F_Cu, B_Cu, F_Cu, In3_Cu, pcbIUScale.mmToIU( 0.8 ) );

    // Track on B_Cu 0.6mm out: inside the 0.8mm bore's clearance, but the bore does not reach B_Cu;
    // against the 0.15mm primary radius the gap is 0.325mm >= 0.25mm -> no violation.
    int x = pcbIUScale.mmToIU( 10.6 );
    CreateTrack( VECTOR2I( x, pcbIUScale.mmToIU( 9 ) ), VECTOR2I( x, pcbIUScale.mmToIU( 11 ) ),
                 B_Cu, trackNet );
    RebuildConnectivity();

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_HOLE_CLEARANCE );
    BOOST_CHECK_EQUAL( violations.size(), 0u );
}


/**
 * A different-net track spaced clear of even the enlarged backdrill bore must not be flagged.
 */
BOOST_FIXTURE_TEST_CASE( DRCBackdrillHoleToCopperClear, BACKDRILL_TEST_FIXTURE )
{
    BOARD_DESIGN_SETTINGS& bds = m_board->GetDesignSettings();
    bds.m_HoleClearance = pcbIUScale.mmToIU( 0.25 );
    bds.m_MinClearance = pcbIUScale.mmToIU( 0.1 );
    bds.m_DRCEngine->InitEngine( wxFileName() );

    int viaNet = GetNetCode( "ViaNet" );
    int trackNet = GetNetCode( "TrackNet" );

    VECTOR2I viaPos( pcbIUScale.mmToIU( 10 ), pcbIUScale.mmToIU( 10 ) );
    CreateBackdrilledVia( viaPos, viaNet, F_Cu, B_Cu, F_Cu, In3_Cu, pcbIUScale.mmToIU( 0.8 ) );

    // Track on F_Cu 1.2mm out: 0.675mm from the 0.4mm bore radius, well over the 0.25mm clearance.
    int x = pcbIUScale.mmToIU( 11.2 );
    CreateTrack( VECTOR2I( x, pcbIUScale.mmToIU( 9 ) ), VECTOR2I( x, pcbIUScale.mmToIU( 11 ) ),
                 F_Cu, trackNet );
    RebuildConnectivity();

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_HOLE_CLEARANCE );
    BOOST_CHECK_EQUAL( violations.size(), 0u );
}


/**
 * Pads carry backdrills too.  A backdrilled pad whose 0.8mm primary drill clears the hole-to-hole
 * minimum against a nearby via, but whose 1.2mm backdrill bore does not, must be flagged.
 */
BOOST_FIXTURE_TEST_CASE( DRCPadBackdrillHoleToHoleTooClose, BACKDRILL_TEST_FIXTURE )
{
    BOARD_DESIGN_SETTINGS& bds = m_board->GetDesignSettings();
    bds.m_HoleToHoleMin = pcbIUScale.mmToIU( 0.3 );
    bds.m_DRCEngine->InitEngine( wxFileName() );

    int netCode = GetNetCode( "TestNet" );

    // Backdrilled pad: 0.8mm primary drill, 1.2mm backdrill bore (F_Cu -> In3_Cu).
    FOOTPRINT* fp = CreateFootprintWithPad( VECTOR2I( 0, 0 ), netCode );
    SetPadBackdrill( fp->Pads().front(), F_Cu, In3_Cu, pcbIUScale.mmToIU( 1.2 ) );

    // Plain via 0.95mm away: 0.3mm drill.  Primary edges are 0.40mm apart (clear); the 1.2mm bore
    // and the 0.3mm drill are only 0.20mm apart, under the 0.3mm minimum.
    PCB_VIA* via = new PCB_VIA( m_board.get() );
    via->SetPosition( VECTOR2I( pcbIUScale.mmToIU( 0.95 ), 0 ) );
    via->SetLayerPair( F_Cu, B_Cu );
    via->SetDrill( pcbIUScale.mmToIU( 0.3 ) );
    via->SetWidth( PADSTACK::ALL_LAYERS, pcbIUScale.mmToIU( 0.6 ) );
    via->SetNetCode( netCode );
    m_board->Add( via );
    RebuildConnectivity();

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_DRILLED_HOLES_TOO_CLOSE );
    BOOST_CHECK_GE( violations.size(), 1u );
}


/**
 * The same backdrilled pad and via spaced so even the bore clears the minimum must not be flagged.
 */
BOOST_FIXTURE_TEST_CASE( DRCPadBackdrillHoleToHoleClear, BACKDRILL_TEST_FIXTURE )
{
    BOARD_DESIGN_SETTINGS& bds = m_board->GetDesignSettings();
    bds.m_HoleToHoleMin = pcbIUScale.mmToIU( 0.3 );
    bds.m_DRCEngine->InitEngine( wxFileName() );

    int netCode = GetNetCode( "TestNet" );

    FOOTPRINT* fp = CreateFootprintWithPad( VECTOR2I( 0, 0 ), netCode );
    SetPadBackdrill( fp->Pads().front(), F_Cu, In3_Cu, pcbIUScale.mmToIU( 1.2 ) );

    // Via 1.6mm away: the 0.6mm bore radius and 0.15mm drill radius are 0.85mm apart, well clear.
    PCB_VIA* via = new PCB_VIA( m_board.get() );
    via->SetPosition( VECTOR2I( pcbIUScale.mmToIU( 1.6 ), 0 ) );
    via->SetLayerPair( F_Cu, B_Cu );
    via->SetDrill( pcbIUScale.mmToIU( 0.3 ) );
    via->SetWidth( PADSTACK::ALL_LAYERS, pcbIUScale.mmToIU( 0.6 ) );
    via->SetNetCode( netCode );
    m_board->Add( via );
    RebuildConnectivity();

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_DRILLED_HOLES_TOO_CLOSE );
    BOOST_CHECK_EQUAL( violations.size(), 0u );
}


/**
 * A backdrilled pad's enlarged bore must be honoured in hole-to-copper clearance too.  A
 * different-net track on a backdrilled layer, clear of the 0.8mm primary drill but inside the
 * 1.2mm bore's clearance, must be flagged.
 */
BOOST_FIXTURE_TEST_CASE( DRCPadBackdrillHoleToCopperTooClose, BACKDRILL_TEST_FIXTURE )
{
    BOARD_DESIGN_SETTINGS& bds = m_board->GetDesignSettings();
    bds.m_HoleClearance = pcbIUScale.mmToIU( 0.25 );
    bds.m_MinClearance = pcbIUScale.mmToIU( 0.1 );
    bds.m_DRCEngine->InitEngine( wxFileName() );

    int padNet = GetNetCode( "PadNet" );
    int trackNet = GetNetCode( "TrackNet" );

    VECTOR2I padPos( pcbIUScale.mmToIU( 10 ), pcbIUScale.mmToIU( 10 ) );
    FOOTPRINT* fp = CreateFootprintWithPad( padPos, padNet );
    // Backdrill bore (1.8mm) wider than the 1.5mm copper land, so on the backdrilled layer the
    // track faces the bore, and on an unaffected layer it clears the copper -- separating the two.
    SetPadBackdrill( fp->Pads().front(), F_Cu, In2_Cu, pcbIUScale.mmToIU( 1.8 ) );

    // Track on backdrilled F_Cu, 1.2mm from the pad centre: 0.175mm from the 0.9mm bore radius
    // (< 0.25mm hole clearance -> violation, but > 0.1mm copper clearance so the hole test still
    // runs) and 0.675mm from the 0.4mm primary radius (clear under the old primary-only model).
    int x = pcbIUScale.mmToIU( 11.2 );
    CreateTrack( VECTOR2I( x, pcbIUScale.mmToIU( 9 ) ), VECTOR2I( x, pcbIUScale.mmToIU( 11 ) ),
                 F_Cu, trackNet );
    RebuildConnectivity();

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_HOLE_CLEARANCE );
    BOOST_CHECK_GE( violations.size(), 1u );
}


/**
 * The same pad and track, but the track is on B_Cu below the In2_Cu must-cut: the bore does not
 * reach it, only the 0.8mm primary drill does, and 0.85mm out that clears -> no violation.  Pins
 * the pad layer-awareness.
 */
BOOST_FIXTURE_TEST_CASE( DRCPadBackdrillHoleToCopperUnaffectedLayer, BACKDRILL_TEST_FIXTURE )
{
    BOARD_DESIGN_SETTINGS& bds = m_board->GetDesignSettings();
    bds.m_HoleClearance = pcbIUScale.mmToIU( 0.25 );
    bds.m_MinClearance = pcbIUScale.mmToIU( 0.1 );
    bds.m_DRCEngine->InitEngine( wxFileName() );

    int padNet = GetNetCode( "PadNet" );
    int trackNet = GetNetCode( "TrackNet" );

    VECTOR2I padPos( pcbIUScale.mmToIU( 10 ), pcbIUScale.mmToIU( 10 ) );
    FOOTPRINT* fp = CreateFootprintWithPad( padPos, padNet );
    SetPadBackdrill( fp->Pads().front(), F_Cu, In2_Cu, pcbIUScale.mmToIU( 1.8 ) );

    // Track on B_Cu 1.2mm out: the bore does not reach B_Cu, so against the 0.4mm primary radius
    // the hole gap is 0.675mm and against the 0.75mm copper land the gap is 0.325mm -- both clear.
    int x = pcbIUScale.mmToIU( 11.2 );
    CreateTrack( VECTOR2I( x, pcbIUScale.mmToIU( 9 ) ), VECTOR2I( x, pcbIUScale.mmToIU( 11 ) ),
                 B_Cu, trackNet );
    RebuildConnectivity();

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_HOLE_CLEARANCE );
    BOOST_CHECK_EQUAL( violations.size(), 0u );
}


/**
 * When a via does not flash a layer, the copper-clearance test substitutes the hole shape for the
 * via.  On a layer that is both unconnected (barrel copper removed by the unconnected-layer mode)
 * and backdrilled, that substitute must be the enlarged bore -- matching what GetEffectiveShape()
 * yields for the same via as the reference item -- not the primary drill.
 *
 * A track/via pair is deduplicated by pointer order and tested in one direction only; the bug is
 * therefore direction-sensitive (like GitLab #24355).  The track is created first so it tends to
 * get the lower heap address and thus be the reference (via as the substitute "other"), the
 * direction the fix affects; a warning fires if the allocator defeats that, in which case the pass
 * is via the via-as-reference direction rather than the substitute.
 */
BOOST_FIXTURE_TEST_CASE( DRCBackdrillUnflashedViaClearanceProxy, BACKDRILL_TEST_FIXTURE )
{
    BOARD_DESIGN_SETTINGS& bds = m_board->GetDesignSettings();
    bds.m_MinClearance = pcbIUScale.mmToIU( 0.4 );
    bds.m_DRCEngine->InitEngine( wxFileName() );

    int viaNet = GetNetCode( "ViaNet" );
    int trackNet = GetNetCode( "TrackNet" );

    VECTOR2I viaPos( pcbIUScale.mmToIU( 10 ), pcbIUScale.mmToIU( 10 ) );

    // Different-net track on In1_Cu, 0.9mm out: 0.275mm from the 0.5mm bore radius (< 0.4mm min
    // clearance -> violation) but 0.625mm from the 0.15mm primary radius (clear under primary-only).
    int x = pcbIUScale.mmToIU( 10.9 );
    PCB_TRACK* track = CreateTrack( VECTOR2I( x, pcbIUScale.mmToIU( 9 ) ),
                                    VECTOR2I( x, pcbIUScale.mmToIU( 11 ) ), In1_Cu, trackNet );

    // Through via (0.3mm drill, 0.6mm copper), inner unconnected copper removed, backdrilled
    // F_Cu -> In2_Cu with a 1.0mm bore.  In1_Cu is inner+unconnected (so the via does not flash it)
    // and inside the backdrill span (so its barrel is bored to 1.0mm).
    PCB_VIA* via = CreateBackdrilledVia( viaPos, viaNet, F_Cu, B_Cu, F_Cu, In2_Cu,
                                         pcbIUScale.mmToIU( 1.0 ) );
    via->Padstack().SetUnconnectedLayerMode( UNCONNECTED_LAYER_MODE::REMOVE_EXCEPT_START_AND_END );
    RebuildConnectivity();

    BOOST_REQUIRE( !via->FlashLayer( In1_Cu ) );
    BOOST_REQUIRE( via->IsBackdrilledOrPostMachined( In1_Cu ) );
    BOOST_WARN_MESSAGE( static_cast<void*>( track ) < static_cast<void*>( via ),
                        "Heap put the via below the track; the substitute direction may not be "
                        "exercised in this run." );

    std::vector<DRC_ITEM> violations = RunDRCForErrorCode( DRCE_CLEARANCE );
    BOOST_CHECK_GE( violations.size(), 1u );
}
