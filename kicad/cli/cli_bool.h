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

#ifndef KICAD_CLI_CLI_BOOL_H
#define KICAD_CLI_CLI_BOOL_H

#include <cctype>
#include <string>

namespace CLI
{
/**
 * Parse a boolean CLI value: yes/no, y/n, true/false, on/off, 1/0 (case-insensitive) into @p aOut.
 *
 * @return true on a recognised value; false otherwise (@p aOut untouched).
 */
inline bool ParseYesNo( const std::string& aStr, bool& aOut )
{
    std::string v;

    for( char c : aStr )
        v += static_cast<char>( std::tolower( static_cast<unsigned char>( c ) ) );

    if( v == "yes" || v == "y" || v == "true" || v == "on" || v == "1" )
    {
        aOut = true;
        return true;
    }

    if( v == "no" || v == "n" || v == "false" || v == "off" || v == "0" )
    {
        aOut = false;
        return true;
    }

    return false;
}
} // namespace CLI

#endif
