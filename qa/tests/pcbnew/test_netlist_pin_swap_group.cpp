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


BOOST_AUTO_TEST_SUITE_END()
