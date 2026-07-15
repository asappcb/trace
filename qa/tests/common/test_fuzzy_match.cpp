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
 * @file test_fuzzy_match.cpp
 * Tests for the command-palette fuzzy matcher (KIFUZZY). The matcher is deterministic and
 * UI-independent; these tests pin its match/no-match contract and the ranking guarantees the
 * palette relies on (word-boundary, contiguity, prefix, and shorter-text tie-breaks).
 */

#include <boost/test/unit_test.hpp>

#include <kicad_fuzzy_match.h>


BOOST_AUTO_TEST_SUITE( FuzzyMatch )


BOOST_AUTO_TEST_CASE( EmptyPatternMatchesEverythingNeutrally )
{
    BOOST_CHECK_EQUAL( KIFUZZY::FuzzyScore( wxEmptyString, wxT( "Anything" ) ), 0 );
    BOOST_CHECK( KIFUZZY::FuzzyMatches( wxEmptyString, wxT( "Swap Pad Nets" ) ) );
}


BOOST_AUTO_TEST_CASE( NonSubsequenceDoesNotMatch )
{
    BOOST_CHECK_EQUAL( KIFUZZY::FuzzyScore( wxT( "xyz" ), wxT( "Swap Pad Nets" ) ),
                       KIFUZZY::NO_MATCH );

    // Out-of-order characters are not a subsequence.
    BOOST_CHECK_EQUAL( KIFUZZY::FuzzyScore( wxT( "tenpad" ), wxT( "Pad Nets" ) ),
                       KIFUZZY::NO_MATCH );
}


BOOST_AUTO_TEST_CASE( SubsequenceMatchesAndIsCaseInsensitive )
{
    BOOST_CHECK( KIFUZZY::FuzzyMatches( wxT( "swap" ), wxT( "Swap Pad Nets" ) ) );
    BOOST_CHECK( KIFUZZY::FuzzyMatches( wxT( "SWAP" ), wxT( "swap pad nets" ) ) );

    // Non-contiguous subsequence: s-p-n across word initials.
    BOOST_CHECK( KIFUZZY::FuzzyMatches( wxT( "spn" ), wxT( "Swap Pad Nets" ) ) );
}


BOOST_AUTO_TEST_CASE( PrefixOutranksMidWordMatch )
{
    // Both contain "swap"; the prefix match should score higher than the mid-string one.
    const int prefix = KIFUZZY::FuzzyScore( wxT( "swap" ), wxT( "Swap Pad Nets" ) );
    const int midStr = KIFUZZY::FuzzyScore( wxT( "swap" ), wxT( "Reassign Gate Swaps" ) );

    BOOST_CHECK( prefix != KIFUZZY::NO_MATCH );
    BOOST_CHECK( midStr != KIFUZZY::NO_MATCH );
    BOOST_CHECK_GT( prefix, midStr );
}


BOOST_AUTO_TEST_CASE( ContiguousOutranksScattered )
{
    const int contiguous = KIFUZZY::FuzzyScore( wxT( "abc" ), wxT( "abcdef" ) );
    const int scattered  = KIFUZZY::FuzzyScore( wxT( "abc" ), wxT( "axbxcx" ) );

    BOOST_CHECK( contiguous != KIFUZZY::NO_MATCH );
    BOOST_CHECK( scattered != KIFUZZY::NO_MATCH );
    BOOST_CHECK_GT( contiguous, scattered );
}


BOOST_AUTO_TEST_CASE( WordBoundaryOutranksInterior )
{
    // "n" at the start of a word ("Nets") should beat "n" buried inside a word ("Announce").
    const int boundary = KIFUZZY::FuzzyScore( wxT( "n" ), wxT( "Pad Nets" ) );
    const int interior = KIFUZZY::FuzzyScore( wxT( "n" ), wxT( "Announce" ) );

    BOOST_CHECK_GT( boundary, interior );
}


BOOST_AUTO_TEST_CASE( CamelCaseHumpCountsAsWordStart )
{
    // The 'n' at a camelCase hump ("PadNets") should get the same word-start bonus a spaced
    // boundary would, and beat the same letter buried mid-word ("Announce").
    const int hump     = KIFUZZY::FuzzyScore( wxT( "n" ), wxT( "PadNets" ) );
    const int interior = KIFUZZY::FuzzyScore( wxT( "n" ), wxT( "Announce" ) );

    BOOST_CHECK_GT( hump, interior );
}


BOOST_AUTO_TEST_CASE( ShorterCandidateWinsOnTie )
{
    // Same match quality (exact prefix "via"); the shorter command name should rank higher.
    const int shortName = KIFUZZY::FuzzyScore( wxT( "via" ), wxT( "Via" ) );
    const int longName  = KIFUZZY::FuzzyScore( wxT( "via" ), wxT( "Via Properties Editor" ) );

    BOOST_CHECK_GT( shortName, longName );
}


BOOST_AUTO_TEST_SUITE_END()
