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

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <base_units.h>
#include <board.h>
#include <footprint.h>
#include <pad.h>

#include <google/protobuf/any.pb.h>

#include <memory>

/**
 * IPC round-trip guardrail for field-bearing board items (issue #55 / #53 finding 4).
 *
 * The API modify path constructs a fresh object, Deserialize()s the proto onto it, then
 * CopyFrom()s it onto the live item.  Any persisted member without a matching proto field is
 * therefore reset to its default on every API round-trip.  This test builds a PAD with
 * distinctive, non-default values on the persisted fields the Pad proto carries, runs
 * Serialize -> Deserialize into a fresh PAD, and asserts the values survive.  A regression that
 * drops a currently-carried field from the proto (or from Serialize/Deserialize) then shows up
 * as a failing assertion.
 *
 * A pure Serialize/Deserialize round-trip can only guard fields the proto already carries, so the
 * second test drives the actual modify sequence the handler performs (Clone -> Deserialize ->
 * CopyFrom) against a persisted field that has *no* proto field, which is the #55 bug proper.
 * Start with PAD; extend to other field-bearing types.
 */

BOOST_AUTO_TEST_SUITE( PadIpcRoundTrip )


BOOST_AUTO_TEST_CASE( PersistedProtoFieldsSurviveSerializeDeserialize )
{
    BOARD      board;
    FOOTPRINT* fp = new FOOTPRINT( &board );
    board.Add( fp );

    PAD* pad = new PAD( fp );
    fp->Add( pad );

    // Distinctive, non-default values on the persisted fields the Pad proto carries.
    pad->SetNumber( wxT( "42" ) );
    pad->SetAttribute( PAD_ATTRIB::PTH );
    pad->SetPosition( VECTOR2I( pcbIUScale.mmToIU( 12.5 ), pcbIUScale.mmToIU( 7.25 ) ) );
    pad->SetLocked( true );
    pad->SetPadToDieLength( pcbIUScale.mmToIU( 1.75 ) );
    pad->SetLocalClearance( pcbIUScale.mmToIU( 0.33 ) );
    pad->SetPinFunction( wxT( "CLK" ) );

    // Serialize -> Deserialize into a fresh pad.
    google::protobuf::Any container;
    pad->Serialize( container );

    PAD restored( fp );
    BOOST_REQUIRE( restored.Deserialize( container ) );

    BOOST_CHECK_EQUAL( restored.GetNumber(), pad->GetNumber() );
    BOOST_CHECK( restored.GetAttribute() == pad->GetAttribute() );
    BOOST_CHECK_EQUAL( restored.GetPosition().x, pad->GetPosition().x );
    BOOST_CHECK_EQUAL( restored.GetPosition().y, pad->GetPosition().y );
    BOOST_CHECK_EQUAL( restored.IsLocked(), pad->IsLocked() );
    BOOST_CHECK_EQUAL( restored.GetPadToDieLength(), pad->GetPadToDieLength() );

    BOOST_REQUIRE( restored.GetLocalClearance().has_value() );
    BOOST_CHECK_EQUAL( *restored.GetLocalClearance(), *pad->GetLocalClearance() );

    BOOST_CHECK_EQUAL( restored.GetPinFunction(), pad->GetPinFunction() );
}


/**
 * The #55 bug proper: a persisted field with no proto field must survive an API modify.
 *
 * Teardrop parameters are written to the board file but have no field in the Pad proto, so they
 * are the exact shape of the hazard: a client that knows nothing about them sends a Pad proto
 * carrying only a new pad number, and the handler must not wipe them.  This drives the same
 * sequence api_handler_board.cpp performs -- Clone -> Deserialize -> CopyFrom -- rather than a
 * plain round-trip, because only that sequence exercises the fix.
 */
BOOST_AUTO_TEST_CASE( PersistedNonProtoFieldSurvivesModify )
{
    BOARD      board;
    FOOTPRINT* fp = new FOOTPRINT( &board );
    board.Add( fp );

    PAD* live = new PAD( fp );
    fp->Add( live );

    live->SetNumber( wxT( "7" ) );
    live->GetTeardropParams().m_Enabled = true;
    live->GetTeardropParams().m_TdMaxLen = pcbIUScale.mmToIU( 3.5 );

    // The incoming request: a pad proto that only changes the number.  It is built from a
    // default-constructed PAD, exactly as a client that has no concept of teardrops would send.
    PAD request( fp );
    request.SetNumber( wxT( "42" ) );

    google::protobuf::Any anyItem;
    request.Serialize( anyItem );

    // The modify path as the handler runs it: seed from a clone of the live item so members the
    // proto does not carry keep their current value.
    std::unique_ptr<BOARD_ITEM> updated( static_cast<BOARD_ITEM*>( live->Clone() ) );
    BOOST_REQUIRE( updated->Deserialize( anyItem ) );
    live->CopyFrom( updated.get() );

    // The proto-carried field is applied...
    BOOST_CHECK_EQUAL( live->GetNumber(), wxT( "42" ) );

    // ...and the persisted field the proto knows nothing about is preserved.
    BOOST_CHECK( live->GetTeardropParams().m_Enabled );
    BOOST_CHECK_EQUAL( live->GetTeardropParams().m_TdMaxLen, pcbIUScale.mmToIU( 3.5 ) );

    // Pin that the assertions above actually discriminate: seeding from a freshly constructed
    // item -- the pre-fix behaviour -- drops the teardrop settings.  If this ever stops holding,
    // the guard above has become vacuous and needs rebuilding on a different field.
    PAD legacy( fp );
    legacy.SetNumber( wxT( "7" ) );
    legacy.GetTeardropParams().m_Enabled = true;

    PAD seededFromDefaults( fp );
    BOOST_REQUIRE( seededFromDefaults.Deserialize( anyItem ) );
    legacy.CopyFrom( &seededFromDefaults );

    BOOST_CHECK( !legacy.GetTeardropParams().m_Enabled );
}


BOOST_AUTO_TEST_SUITE_END()
