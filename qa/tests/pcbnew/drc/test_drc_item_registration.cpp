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
 * @file test_drc_item_registration.cpp
 *
 * Guardrail for adding a new DRC error code (issue #54). Registering a new DRCE_* is a multi-site
 * change; forgetting the DRC_ITEM::allItemTypes registration fails silently -- the violation is
 * invisible in the Violation Severity UI and its exclusions are dropped on reload (because
 * DRC_ITEM::Create(wxString) returns null for an unregistered code). This turns that omission into
 * a red test at commit time.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <drc/drc_item.h>

#include <memory>


BOOST_AUTO_TEST_SUITE( DrcItemRegistration )


BOOST_AUTO_TEST_CASE( EveryViolationCodeRoundTripsExclusions )
{
    for( int code = DRCE_FIRST; code <= DRCE_LAST; ++code )
    {
        std::shared_ptr<DRC_ITEM> byCode = DRC_ITEM::Create( code );

        // Some enum values are pseudo-codes that only carry a severity setting (e.g.
        // DRCE_TUNING_PROFILE_IMPLICIT_RULES, "Pseudo-code for setting severities") and are
        // intentionally not produced by the integer factory. They never become real violations,
        // so there is nothing to restore; skip them rather than false-positive on them.
        if( !byCode )
            continue;

        // Every real violation code (one with a settings key) must round-trip through the wxString
        // factory -- the path that restores DRC exclusions from a project. A code that fails to
        // round-trip is not registered in allItemTypes: its exclusions are silently dropped on
        // reload and it never appears in the Violation Severity UI (the #42 must-fix). Internal
        // codes carry no settings key and are intentionally exempt.
        wxString key = byCode->GetSettingsKey();

        if( key.IsEmpty() )
            continue;

        std::shared_ptr<DRC_ITEM> byKey = DRC_ITEM::Create( key );

        wxString notRegistered = wxString::Format( "DRC code %d (settings key '%s') is not registered in "
                                                   "DRC_ITEM::allItemTypes; its exclusions will be dropped on reload "
                                                   "and it will be invisible in the severity UI",
                                                   code, key );
        BOOST_CHECK_MESSAGE( byKey != nullptr, notRegistered );

        if( byKey )
        {
            wxString duplicateKey = wxString::Format( "settings key '%s' maps to code %d, not %d (duplicate key)", key,
                                                      byKey->GetErrorCode(), code );
            BOOST_CHECK_MESSAGE( byKey->GetErrorCode() == code, duplicateKey );
        }
    }
}


BOOST_AUTO_TEST_SUITE_END()
