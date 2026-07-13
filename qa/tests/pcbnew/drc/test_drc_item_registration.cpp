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

#include <memory>

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <drc/drc_item.h>

/**
 * Guardrail for the DRC error-code registration cascade (issue #54 / #53 finding 3).
 *
 * Adding a new DRCE_* code requires touching several parallel sites.  Two of them fail
 * *silently* when forgotten:
 *   - DRC_ITEM::allItemTypes  -- the code becomes invisible in the Violation Severity UI and its
 *                                exclusions are silently dropped on reload, because
 *                                DRC_ITEM::Create( wxString ) cannot resolve an unregistered key.
 *   - DRC_ITEM::Create( int ) -- returns nullptr for a code with no switch case.
 *
 * This meta-test walks the whole DRCE_FIRST..DRCE_LAST range and turns either omission into a
 * red test at commit time instead of a ship-and-discover-later bug.  It is purely additive (no
 * registration refactor), matching the fork's close-upstream-tracking posture.
 */

BOOST_AUTO_TEST_SUITE( DRCItemRegistration )


BOOST_AUTO_TEST_CASE( EveryCodeHasIntFactoryAndRoundTripsThroughSettingsKey )
{
    for( int code = DRCE_FIRST; code <= DRCE_LAST; ++code )
    {
        // (1) The integer factory must know every code in the range.
        std::shared_ptr<DRC_ITEM> byCode = DRC_ITEM::Create( code );

        BOOST_REQUIRE_MESSAGE( byCode,
                "DRCE code " << code << " has no DRC_ITEM::Create( int ) mapping "
                "(add a case in drc_item.cpp)" );

        BOOST_CHECK_MESSAGE( byCode->GetErrorCode() == code,
                "DRC_ITEM::Create( " << code << " ) returned an item whose error code is "
                << byCode->GetErrorCode() );

        // (2) Any code that carries a settings key must resolve back through the string factory,
        //     which scans allItemTypes.  A code that is missing from allItemTypes still has a key
        //     on its static member (so Create(int) fills it in above), but Create(wxString) then
        //     returns null -- exactly the otherwise-silent failure this test exists to catch.
        //     Codes with no settings key (e.g. purely-internal ones) are skipped so we don't
        //     false-match an empty-key heading row.
        const wxString key = byCode->GetSettingsKey();

        if( key.IsEmpty() )
            continue;

        std::shared_ptr<DRC_ITEM> byKey = DRC_ITEM::Create( key );

        BOOST_CHECK_MESSAGE( byKey,
                "DRCE code " << code << " (settings key '" << key
                << "') is not registered in DRC_ITEM::allItemTypes "
                "(add it to the vector in drc_item.cpp)" );

        if( byKey )
        {
            BOOST_CHECK_MESSAGE( byKey->GetErrorCode() == code,
                    "settings key '" << key << "' resolves to error code "
                    << byKey->GetErrorCode() << ", expected " << code );
        }
    }
}


BOOST_AUTO_TEST_SUITE_END()
