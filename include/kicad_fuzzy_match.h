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

#ifndef KICAD_FUZZY_MATCH_H
#define KICAD_FUZZY_MATCH_H

#include <vector>
#include <wx/string.h>
#include <kicommon.h>

/**
 * Lightweight subsequence fuzzy matcher used by the command palette (and usable by any
 * type-to-filter list). The scoring is deterministic and case-insensitive so it can be unit
 * tested without a UI.
 *
 * A match requires @p aPattern to appear as an (in-order, possibly non-contiguous) subsequence
 * of @p aText. Scoring rewards, in rough order of weight:
 *   - matches at a word boundary (start of string, or after a separator/camelCase hump),
 *   - contiguous runs of matched characters,
 *   - small gaps between matched characters,
 *   - an exact prefix, and shorter candidate text (as tie-breakers).
 */
namespace KIFUZZY
{
/// Sentinel returned by FuzzyScore() when @p aPattern is not a subsequence of the text.
static constexpr int NO_MATCH = -1;

/**
 * Score how well @p aText matches @p aPattern.
 *
 * @return a non-negative score (higher is a better match), or NO_MATCH if @p aPattern is not a
 *         subsequence of @p aText. An empty pattern matches everything with a neutral score of 0.
 */
KICOMMON_API int FuzzyScore( const wxString& aPattern, const wxString& aText );

/**
 * Score overload that also reports which character indices of @p aText were matched, in order,
 * so callers (e.g. the command palette) can highlight them. @p aMatchedPositions is cleared and
 * filled only on a match; it is left untouched when the result is NO_MATCH.
 */
KICOMMON_API int FuzzyScore( const wxString& aPattern, const wxString& aText,
                             std::vector<int>& aMatchedPositions );

/**
 * Convenience predicate: true when @p aPattern is a (fuzzy) subsequence of @p aText.
 */
KICOMMON_API bool FuzzyMatches( const wxString& aPattern, const wxString& aText );
}

#endif // KICAD_FUZZY_MATCH_H
