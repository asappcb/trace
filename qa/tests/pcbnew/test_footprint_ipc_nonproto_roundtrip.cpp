/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * @file test_footprint_ipc_nonproto_roundtrip.cpp
 *
 * Guardrail for the IPC "modify silent-wipe" footgun (#55; design-findings register #53, items 4
 * and 6). The API modify path deserializes a proto onto a live board item, so any persisted C++
 * field the proto does NOT carry is at risk of being reset. `FOOTPRINT::m_unitInfo` (per-unit gate
 * metadata added in #57) is exactly such a field: it is persisted but has no proto field.
 *
 * This pins that fact down and guards the invariant the modify fix (#63, "seed Deserialize from a
 * clone of the live item") relies on -- Deserialize must not disturb a non-proto field. If someone
 * adds a proto field for m_unitInfo, or makes Deserialize reset it, this test flags it so the
 * modify path can be revisited rather than silently dropping data.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <footprint.h>

#include <google/protobuf/any.pb.h>

#include <memory>


BOOST_AUTO_TEST_SUITE( FootprintIpcNonProtoRoundtrip )


BOOST_AUTO_TEST_CASE( DeserializePreservesNonProtoUnitInfo )
{
    FOOTPRINT original( nullptr );

    FOOTPRINT::FP_UNIT_INFO unit;
    unit.m_unitName = wxT( "A" );
    unit.m_pins = { wxT( "1" ), wxT( "2" ), wxT( "3" ) };
    original.SetUnitInfo( { unit } );
    original.SetUnitsInterchangeable( true );

    google::protobuf::Any proto;
    original.Serialize( proto );

    // (1) m_unitInfo is genuinely absent from the proto: a fresh footprint deserialized from it has
    //     no unit info. This is precisely why the old fresh-object modify path silently wiped it.
    FOOTPRINT fresh( nullptr );
    BOOST_REQUIRE( fresh.Deserialize( proto ) );
    BOOST_CHECK_MESSAGE( fresh.GetUnitInfo().empty(),
                         "m_unitInfo must not be carried by the IPC proto (guards #55's premise)" );

    // (2) The modify fix seeds Deserialize from a Clone() of the live footprint, which keeps
    //     m_unitInfo; Deserialize (additive for non-proto fields) must leave it intact.
    std::unique_ptr<FOOTPRINT> updated( static_cast<FOOTPRINT*>( original.Clone() ) );
    BOOST_REQUIRE( updated->Deserialize( proto ) );

    BOOST_REQUIRE_EQUAL( updated->GetUnitInfo().size(), 1u );
    BOOST_CHECK_EQUAL( updated->GetUnitInfo()[0].m_unitName, wxString( wxT( "A" ) ) );
    BOOST_REQUIRE_EQUAL( updated->GetUnitInfo()[0].m_pins.size(), 3u );
    BOOST_CHECK_EQUAL( updated->GetUnitInfo()[0].m_pins[1], wxString( wxT( "2" ) ) );
    BOOST_CHECK( updated->AreUnitsInterchangeable() );
}


BOOST_AUTO_TEST_SUITE_END()
