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
 * Reader half of the gate/pin-swap equivalence transport (epic #20, P0): the KiCad netlist reader
 * must parse the per-node (pin_swap_group "<unit> <index>") token onto the component net, and a
 * node without the token must default to not-swap-eligible.
 */

#include <boost/test/unit_test.hpp>

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <pcbnew/netlist_reader/pcb_netlist.h>
#include <pcbnew/netlist_reader/netlist_reader.h>
#include <pcbnew/netlist_reader/board_netlist_updater.h>

#include <board.h>
#include <footprint.h>
#include <pad.h>
#include <lib_id.h>
#include <settings/settings_manager.h>
#include <tool/tool_manager.h>
#include <pcbnew_utils/board_test_utils.h>

#include <wx/filename.h>

#include <fstream>
#include <memory>


BOOST_AUTO_TEST_SUITE( NetlistPinSwapGroup )


BOOST_AUTO_TEST_CASE( ReaderImportsPinSwapGroup )
{
    wxString path = wxFileName::CreateTempFileName( wxT( "kicad_swap_netlist" ) );

    {
        std::ofstream out( path.ToStdString() );
        out << "(export (version \"E\")\n"
               "  (components\n"
               "    (comp (ref \"U1\") (value \"TL072\")\n"
               "      (footprint \"Package_SO:SOIC-8\")\n"
               "      (libsource (lib \"x\") (part \"TL072\"))))\n"
               "  (nets\n"
               "    (net (code \"1\") (name \"N1\")\n"
               "      (node (ref \"U1\") (pin \"3\") (pinfunction \"In+\") (pintype \"input\")"
               " (pin_swap_group \"1 0\")))\n"
               "    (net (code \"2\") (name \"N2\")\n"
               "      (node (ref \"U1\") (pin \"5\") (pintype \"input\")))))\n";
    }

    NETLIST netlist;
    std::unique_ptr<NETLIST_READER> reader(
            NETLIST_READER::GetNetlistReader( &netlist, path, wxEmptyString ) );
    BOOST_REQUIRE( reader );
    reader->LoadNetlist();

    COMPONENT* u1 = netlist.GetComponentByReference( wxT( "U1" ) );
    BOOST_REQUIRE( u1 );

    // The node that carried (pin_swap_group "1 0") imports unit 1, index 0.
    const COMPONENT_NET& n3 = u1->GetNet( wxT( "3" ) );
    BOOST_CHECK_EQUAL( n3.GetPinSwapUnit(), 1 );
    BOOST_CHECK_EQUAL( n3.GetPinSwapIndex(), 0 );

    // The node without the token defaults to not-swap-eligible (index < 0).
    const COMPONENT_NET& n5 = u1->GetNet( wxT( "5" ) );
    BOOST_CHECK_EQUAL( n5.GetPinSwapUnit(), 0 );
    BOOST_CHECK_EQUAL( n5.GetPinSwapIndex(), -1 );

    wxRemoveFile( path );
}


// The board netlist updater must land the swap group on the actual pad, and clear a stale group
// when a subsequent update no longer carries one.
BOOST_AUTO_TEST_CASE( UpdaterAppliesAndClearsPinSwapGroup )
{
    SETTINGS_MANAGER settingsManager;
    settingsManager.LoadProject( "" );

    std::unique_ptr<BOARD> board = std::make_unique<BOARD>();
    board->SetProject( &settingsManager.Prj() );

    LIB_ID fpid;
    BOOST_REQUIRE_EQUAL( fpid.Parse( wxS( "TestLib:U" ) ), -1 );

    FOOTPRINT* fp = new FOOTPRINT( board.get() );
    fp->SetReference( wxS( "U1" ) );
    fp->SetFPID( fpid );
    board->Add( fp );

    PAD* pad = new PAD( fp );
    pad->SetNumber( wxS( "3" ) );
    fp->Add( pad );

    BOOST_REQUIRE( !pad->IsPinSwapEligible() );

    TOOL_MANAGER toolMgr;
    toolMgr.SetEnvironment( board.get(), nullptr, nullptr, nullptr, nullptr );
    toolMgr.RegisterTool( new KI_TEST::DUMMY_TOOL() );

    // First update: the net for pad 3 carries a swap group (unit 1, index 0).
    {
        NETLIST netlist;
        KIID    kiid;
        COMPONENT* comp = new COMPONENT( fpid, wxS( "U1" ), wxS( "U1" ), KIID_PATH(),
                                         std::vector<KIID>{ kiid } );
        comp->AddNet( wxS( "3" ), wxS( "N1" ), wxS( "In+" ), wxS( "input" ), 1, 0 );
        netlist.AddComponent( comp );

        BOARD_NETLIST_UPDATER updater( &toolMgr, board.get() );
        updater.SetReplaceFootprints( false );
        updater.SetDeleteUnusedFootprints( false );
        BOOST_REQUIRE( updater.UpdateNetlist( netlist ) );

        BOOST_CHECK( pad->IsPinSwapEligible() );
        BOOST_CHECK_EQUAL( pad->GetPinSwapUnit(), 1 );
        BOOST_CHECK_EQUAL( pad->GetPinSwapIndex(), 0 );
    }

    // Second update: the same net no longer carries a swap group -> the stale group is cleared.
    {
        NETLIST netlist;
        KIID    kiid;
        COMPONENT* comp = new COMPONENT( fpid, wxS( "U1" ), wxS( "U1" ), KIID_PATH(),
                                         std::vector<KIID>{ kiid } );
        comp->AddNet( wxS( "3" ), wxS( "N1" ), wxS( "In+" ), wxS( "input" ) );
        netlist.AddComponent( comp );

        BOARD_NETLIST_UPDATER updater( &toolMgr, board.get() );
        updater.SetReplaceFootprints( false );
        updater.SetDeleteUnusedFootprints( false );
        BOOST_REQUIRE( updater.UpdateNetlist( netlist ) );

        BOOST_CHECK( !pad->IsPinSwapEligible() );
        BOOST_CHECK_EQUAL( pad->GetPinSwapIndex(), -1 );
    }
}


BOOST_AUTO_TEST_SUITE_END()
