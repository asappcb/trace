/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers
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

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <schematic_utils/schematic_file_util.h>

#include <set>
#include <vector>

#include <schematic.h>
#include <sch_screen.h>
#include <sch_symbol.h>
#include <lib_symbol.h>
#include <settings/settings_manager.h>
#include <netlist_exporter_xml.h>

#include <wx/filename.h>
#include <wx/xml/xml.h>

struct XML_STACKED_PIN_FIXTURE
{
    XML_STACKED_PIN_FIXTURE() {}

    SETTINGS_MANAGER           m_settingsManager;
    std::unique_ptr<SCHEMATIC> m_schematic;
};

static std::set<wxString> as_set( const std::initializer_list<const char*>& init )
{
    std::set<wxString> out;
    for( const char* s : init )
        out.emplace( wxString::FromUTF8( s ) );
    return out;
}

static wxXmlNode* find_child( wxXmlNode* parent, const wxString& name )
{
    for( wxXmlNode* child = parent->GetChildren(); child; child = child->GetNext() )
    {
        if( child->GetName() == name )
            return child;
    }

    return nullptr;
}

static std::vector<wxXmlNode*> find_children( wxXmlNode* parent, const wxString& name )
{
    std::vector<wxXmlNode*> out;

    for( wxXmlNode* child = parent->GetChildren(); child; child = child->GetNext() )
    {
        if( child->GetName() == name )
            out.push_back( child );
    }

    return out;
}

static wxXmlNode* find_component( wxXmlNode* components, const wxString& ref )
{
    for( wxXmlNode* comp : find_children( components, wxT( "comp" ) ) )
    {
        if( comp->GetAttribute( wxT( "ref" ), wxEmptyString ) == ref )
            return comp;
    }

    return nullptr;
}

static wxXmlNode* find_unit( wxXmlNode* units, const wxString& name )
{
    for( wxXmlNode* unit : find_children( units, wxT( "unit" ) ) )
    {
        if( unit->GetAttribute( wxT( "name" ), wxEmptyString ) == name )
            return unit;
    }

    return nullptr;
}

static std::vector<wxString> get_pin_numbers( wxXmlNode* unit )
{
    std::vector<wxString> pins;
    wxXmlNode*            pinList = find_child( unit, wxT( "pins" ) );

    BOOST_REQUIRE( pinList );

    if( !pinList )
        return pins;

    for( wxXmlNode* pin : find_children( pinList, wxT( "pin" ) ) )
        pins.push_back( pin->GetAttribute( wxT( "num" ), wxEmptyString ) );

    return pins;
}

BOOST_FIXTURE_TEST_CASE( NetlistExporterXML_StackedPinNomenclature, XML_STACKED_PIN_FIXTURE )
{
    // Load schematic with stacked pin numbers
    KI_TEST::LoadSchematic( m_settingsManager, wxT( "stacked_pin_nomenclature" ), m_schematic );

    // Write XML netlist to a test file next to the project
    wxFileName netFile = m_schematic->Project().GetProjectFullName();
    netFile.SetName( netFile.GetName() + wxT( "_xml_test" ) );
    netFile.SetExt( wxT( "xml" ) );

    if( wxFileExists( netFile.GetFullPath() ) )
        wxRemoveFile( netFile.GetFullPath() );

    WX_STRING_REPORTER reporter;
    std::unique_ptr<NETLIST_EXPORTER_XML> exporter =
            std::make_unique<NETLIST_EXPORTER_XML>( m_schematic.get() );

    bool success = exporter->WriteNetlist( netFile.GetFullPath(), 0, reporter );
    BOOST_REQUIRE( success && reporter.GetMessages().IsEmpty() );

    // Parse the XML back
    wxXmlDocument xdoc;
    BOOST_REQUIRE( xdoc.Load( netFile.GetFullPath() ) );

    wxXmlNode* root = xdoc.GetRoot();
    BOOST_REQUIRE( root );

    wxXmlNode* nets = find_child( root, wxT( "nets" ) );
    BOOST_REQUIRE( nets );

    // Collect pin sets for R1 on each power net
    std::set<wxString> setA;
    std::set<wxString> setB;
    int                foundSets = 0;

    for( wxXmlNode* net : find_children( nets, wxT( "net" ) ) )
    {
        wxString netName = net->GetAttribute( wxT( "name" ), wxEmptyString );
        if( netName != wxT( "VCC" ) && netName != wxT( "GND" ) )
            continue;

        std::set<wxString>* target = ( foundSets == 0 ? &setA : &setB );

        for( wxXmlNode* node : find_children( net, wxT( "node" ) ) )
        {
            if( node->GetAttribute( wxT( "ref" ), wxEmptyString ) != wxT( "R1" ) )
                continue;

            wxString pin = node->GetAttribute( wxT( "pin" ), wxEmptyString );
            wxString pinfunction = node->GetAttribute( wxT( "pinfunction" ), wxEmptyString );
            wxString pintype = node->GetAttribute( wxT( "pintype" ), wxEmptyString );

            // Expect pinfunction to equal the expanded number when base name is empty
            BOOST_CHECK_EQUAL( pinfunction, pin );
            // Expect plain passive type (no +no_connect on these nets)
            BOOST_CHECK_EQUAL( pintype, wxT( "passive" ) );

            target->insert( pin );
        }

        foundSets++;
    }

    // We should have found two power nets with R1 nodes
    BOOST_REQUIRE_EQUAL( foundSets, 2 );

    // Expect one side to be 1..5 and the other to be 6,7,9,10,11 (order independent)
    const std::set<wxString> expectedTop = as_set( { "1", "2", "3", "4", "5" } );
    const std::set<wxString> expectedBot = as_set( { "6", "7", "9", "10", "11" } );

    bool matchA = ( setA == expectedTop && setB == expectedBot );
    bool matchB = ( setA == expectedBot && setB == expectedTop );
    BOOST_CHECK( matchA || matchB );

    // Cleanup test artifact
    wxRemoveFile( netFile.GetFullPath() );
}


// OK, this isn't really a "stacked pin" test, but it's a convenient place to verify that the XML netlist exporter is correctly
// resolving per-unit lib symbol metadata for components with multiple placed units.
BOOST_FIXTURE_TEST_CASE( NetlistExporterXML_UsesPerUnitResolvedLibraryMetadata, XML_STACKED_PIN_FIXTURE )
{
    KI_TEST::LoadSchematic( m_settingsManager, wxT( "netlist_exporter_unit_metadata_per_unit" ), m_schematic );

    wxFileName netFile = m_schematic->Project().GetProjectFullName();
    netFile.SetName( netFile.GetName() + wxT( "_xml_unit_metadata_test" ) );
    netFile.SetExt( wxT( "xml" ) );

    if( wxFileExists( netFile.GetFullPath() ) )
        wxRemoveFile( netFile.GetFullPath() );

    WX_STRING_REPORTER                    reporter;
    std::unique_ptr<NETLIST_EXPORTER_XML> exporter = std::make_unique<NETLIST_EXPORTER_XML>( m_schematic.get() );

    bool success = exporter->WriteNetlist( netFile.GetFullPath(), 0, reporter );
    BOOST_REQUIRE( success && reporter.GetMessages().IsEmpty() );

    wxXmlDocument xdoc;
    BOOST_REQUIRE( xdoc.Load( netFile.GetFullPath() ) );

    wxXmlNode* root = xdoc.GetRoot();
    BOOST_REQUIRE( root );

    wxXmlNode* components = find_child( root, wxT( "components" ) );
    BOOST_REQUIRE( components );

    wxXmlNode* u1 = find_component( components, wxT( "U1" ) );
    BOOST_REQUIRE( u1 );

    wxXmlNode* units = find_child( u1, wxT( "units" ) );
    BOOST_REQUIRE( units );

    wxXmlNode* unitA = find_unit( units, wxT( "A" ) );
    wxXmlNode* unitB = find_unit( units, wxT( "B" ) );
    wxXmlNode* unitC = find_unit( units, wxT( "C" ) );

    BOOST_REQUIRE( unitA );
    BOOST_REQUIRE( unitB );
    BOOST_REQUIRE( unitC );

    const std::vector<wxString> expectedUnitA{ wxT( "3" ), wxT( "2" ), wxT( "1" ) };
    const std::vector<wxString> expectedUnitB{ wxT( "6" ), wxT( "5" ), wxT( "7" ) };
    const std::vector<wxString> expectedUnitC{ wxT( "8" ), wxT( "4" ) };

    BOOST_CHECK( get_pin_numbers( unitA ) == expectedUnitA );
    BOOST_CHECK( get_pin_numbers( unitB ) == expectedUnitB );
    BOOST_CHECK( get_pin_numbers( unitC ) == expectedUnitC );

    wxRemoveFile( netFile.GetFullPath() );
}


// The XML netlist must carry pin_swap_group on the net nodes of interchangeable-unit symbols so a
// layout-side optimizer can reassign gates. For the TL072 (two 3-pin amp sections A/B plus a 2-pin
// power unit C), the amp pins must be marked gate-swappable (unit 1 or 2) while the power pins must
// not -- the mismatched-size power unit is excluded.
BOOST_FIXTURE_TEST_CASE( NetlistExporterXML_EmitsGateSwapGroups, XML_STACKED_PIN_FIXTURE )
{
    KI_TEST::LoadSchematic( m_settingsManager, wxT( "netlist_exporter_unit_metadata_per_unit" ), m_schematic );

    // The fixture's TL072 is cached with its units locked; unlock the interchangeable sections so
    // the gate-swap emission path is exercised deterministically regardless of the cached state.
    for( const SCH_SHEET_PATH& sheet : m_schematic->Hierarchy() )
    {
        for( SCH_ITEM* item : sheet.LastScreen()->Items().OfType( SCH_SYMBOL_T ) )
        {
            SCH_SYMBOL* sym = static_cast<SCH_SYMBOL*>( item );

            if( sym->GetLibSymbolRef() && sym->GetLibSymbolRef()->GetUnitCount() > 1 )
                sym->GetLibSymbolRef()->LockUnits( false );
        }
    }

    wxFileName netFile = m_schematic->Project().GetProjectFullName();
    netFile.SetName( netFile.GetName() + wxT( "_xml_swap_group_test" ) );
    netFile.SetExt( wxT( "xml" ) );

    if( wxFileExists( netFile.GetFullPath() ) )
        wxRemoveFile( netFile.GetFullPath() );

    WX_STRING_REPORTER                    reporter;
    std::unique_ptr<NETLIST_EXPORTER_XML> exporter = std::make_unique<NETLIST_EXPORTER_XML>( m_schematic.get() );

    BOOST_REQUIRE( exporter->WriteNetlist( netFile.GetFullPath(), 0, reporter )
                   && reporter.GetMessages().IsEmpty() );

    wxXmlDocument xdoc;
    BOOST_REQUIRE( xdoc.Load( netFile.GetFullPath() ) );

    wxXmlNode* nets = find_child( xdoc.GetRoot(), wxT( "nets" ) );
    BOOST_REQUIRE( nets );

    // Collect, for U1, the pin_swap_group of every emitted node (only pins that carry one).
    std::map<wxString, wxString> u1SwapByPin;

    for( wxXmlNode* net : find_children( nets, wxT( "net" ) ) )
    {
        for( wxXmlNode* node : find_children( net, wxT( "node" ) ) )
        {
            if( node->GetAttribute( wxT( "ref" ) ) != wxT( "U1" ) )
                continue;

            wxString group;

            if( node->GetAttribute( wxT( "pin_swap_group" ), &group ) )
                u1SwapByPin[node->GetAttribute( wxT( "pin" ) )] = group;
        }
    }

    // The amp sections are marked; the 2-pin power unit's pins (4, 8) are not.
    BOOST_REQUIRE( !u1SwapByPin.empty() );
    BOOST_CHECK( u1SwapByPin.count( wxT( "4" ) ) == 0 );
    BOOST_CHECK( u1SwapByPin.count( wxT( "8" ) ) == 0 );

    // Every marked pin belongs to an amp section (unit 1 or 2), never the power unit (3).
    for( const auto& [pin, group] : u1SwapByPin )
    {
        long unit = 0;
        BOOST_REQUIRE( group.BeforeFirst( ' ' ).ToLong( &unit ) );
        BOOST_CHECK_MESSAGE( unit == 1 || unit == 2,
                             "pin " << pin << " got unexpected swap unit " << unit );
    }

    // The first-position input of each amp section (pin 3 in unit A, pin 6 in unit B) shares index
    // 0, proving the cross-unit position correspondence is consistent.
    if( u1SwapByPin.count( wxT( "3" ) ) && u1SwapByPin.count( wxT( "6" ) ) )
    {
        BOOST_CHECK_EQUAL( u1SwapByPin[wxT( "3" )], wxT( "1 0" ) );
        BOOST_CHECK_EQUAL( u1SwapByPin[wxT( "6" )], wxT( "2 0" ) );
    }

    wxRemoveFile( netFile.GetFullPath() );
}


// The inverse of the above: when the symbol's units are LOCKED (not interchangeable), the exporter
// must emit no pin_swap_group at all. The fixture's TL072 is cached with its units locked, so a
// plain export (no in-test unlock) must produce zero swap groups.
BOOST_FIXTURE_TEST_CASE( NetlistExporterXML_LockedUnitsEmitNoSwapGroup, XML_STACKED_PIN_FIXTURE )
{
    KI_TEST::LoadSchematic( m_settingsManager, wxT( "netlist_exporter_unit_metadata_per_unit" ), m_schematic );

    // Sanity: the fixture's multi-unit symbol really is locked (otherwise this test proves nothing).
    bool sawLockedMultiUnit = false;

    for( const SCH_SHEET_PATH& sheet : m_schematic->Hierarchy() )
    {
        for( SCH_ITEM* item : sheet.LastScreen()->Items().OfType( SCH_SYMBOL_T ) )
        {
            SCH_SYMBOL* sym = static_cast<SCH_SYMBOL*>( item );

            if( sym->GetLibSymbolRef() && sym->GetLibSymbolRef()->GetUnitCount() > 1
                && sym->GetLibSymbolRef()->UnitsLocked() )
            {
                sawLockedMultiUnit = true;
            }
        }
    }

    BOOST_REQUIRE( sawLockedMultiUnit );

    wxFileName netFile = m_schematic->Project().GetProjectFullName();
    netFile.SetName( netFile.GetName() + wxT( "_xml_locked_swap_test" ) );
    netFile.SetExt( wxT( "xml" ) );

    if( wxFileExists( netFile.GetFullPath() ) )
        wxRemoveFile( netFile.GetFullPath() );

    WX_STRING_REPORTER                    reporter;
    std::unique_ptr<NETLIST_EXPORTER_XML> exporter = std::make_unique<NETLIST_EXPORTER_XML>( m_schematic.get() );

    BOOST_REQUIRE( exporter->WriteNetlist( netFile.GetFullPath(), 0, reporter )
                   && reporter.GetMessages().IsEmpty() );

    wxXmlDocument xdoc;
    BOOST_REQUIRE( xdoc.Load( netFile.GetFullPath() ) );

    wxXmlNode* nets = find_child( xdoc.GetRoot(), wxT( "nets" ) );
    BOOST_REQUIRE( nets );

    for( wxXmlNode* net : find_children( nets, wxT( "net" ) ) )
    {
        for( wxXmlNode* node : find_children( net, wxT( "node" ) ) )
            BOOST_CHECK( !node->HasAttribute( wxT( "pin_swap_group" ) ) );
    }

    wxRemoveFile( netFile.GetFullPath() );
}
