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

#include <kicad_fuzzy_match.h>

#include <algorithm>
#include <cwctype>


namespace
{
bool isSeparator( wxUniChar aChar )
{
    return aChar == ' ' || aChar == '.' || aChar == '-' || aChar == '_' || aChar == '/'
           || aChar == ':' || aChar == '(' || aChar == ')' || aChar == '&';
}

bool isUpperChar( wxUniChar aChar )
{
    return std::iswupper( static_cast<wint_t>( aChar.GetValue() ) ) != 0;
}

bool isLowerOrDigit( wxUniChar aChar )
{
    const wint_t w = static_cast<wint_t>( aChar.GetValue() );
    return std::iswlower( w ) != 0 || std::iswdigit( w ) != 0;
}

/// A match at @p aIdx starts a "word": string start, after a separator, or a camelCase hump
/// (lower/digit → upper). Boundary detection needs the ORIGINAL case, so it takes the raw text.
bool isWordStart( const wxString& aRawText, size_t aIdx )
{
    if( aIdx == 0 )
        return true;

    if( isSeparator( aRawText[aIdx - 1] ) )
        return true;

    return isLowerOrDigit( aRawText[aIdx - 1] ) && isUpperChar( aRawText[aIdx] );
}
}


namespace
{
/// Shared scoring core. When @p aPositions is non-null it is filled (only on a match) with the
/// matched character indices, in order.
int fuzzyScoreImpl( const wxString& aPattern, const wxString& aText, std::vector<int>* aPositions )
{
    if( aPattern.IsEmpty() )
        return 0;

    const wxString pattern = aPattern.Lower();
    const wxString text    = aText.Lower();  // matched case-insensitively...

    int    score    = 0;
    int    run      = 0;   // length of the current contiguous matched run
    int    lastIdx  = -2;  // index in text of the previously matched char
    size_t searchAt = 0;

    std::vector<int> positions;

    for( size_t pi = 0; pi < pattern.length(); ++pi )
    {
        const size_t foundAt = text.find( pattern[pi], searchAt );

        if( foundAt == wxString::npos )
            return KIFUZZY::NO_MATCH;

        int bonus = 0;

        // Word-boundary bonus: string start, after a separator, or a camelCase hump. Boundary
        // detection uses the original-case @p aText (text is already lowercased for matching).
        if( isWordStart( aText, foundAt ) )
            bonus += 15;

        // Contiguous-run bonus: escalates so tightly-packed matches beat scattered ones.
        if( static_cast<int>( foundAt ) == lastIdx + 1 )
        {
            ++run;
            bonus += 5 * run;
        }
        else
        {
            run = 0;
        }

        const int gap = static_cast<int>( foundAt ) - static_cast<int>( searchAt );
        score += 10 + bonus - std::min( gap, 8 );

        if( aPositions )
            positions.push_back( static_cast<int>( foundAt ) );

        lastIdx  = static_cast<int>( foundAt );
        searchAt = foundAt + 1;
    }

    // Tie-breakers: reward an exact prefix and shorter candidates.
    if( text.StartsWith( pattern ) )
        score += 25;

    score += std::max( 0, 15 - static_cast<int>( text.length() ) / 2 );

    if( aPositions )
        *aPositions = std::move( positions );

    return score;
}
}


int KIFUZZY::FuzzyScore( const wxString& aPattern, const wxString& aText )
{
    return fuzzyScoreImpl( aPattern, aText, nullptr );
}


int KIFUZZY::FuzzyScore( const wxString& aPattern, const wxString& aText,
                         std::vector<int>& aMatchedPositions )
{
    return fuzzyScoreImpl( aPattern, aText, &aMatchedPositions );
}


bool KIFUZZY::FuzzyMatches( const wxString& aPattern, const wxString& aText )
{
    return FuzzyScore( aPattern, aText ) != KIFUZZY::NO_MATCH;
}
