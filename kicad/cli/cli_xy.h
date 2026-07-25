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

#ifndef KICAD_CLI_CLI_XY_H
#define KICAD_CLI_CLI_XY_H

#include <cstddef>
#include <stdexcept>
#include <string>

namespace CLI
{
/**
 * Parse a strict "X,Y" pair of decimal millimetres into @p aX / @p aY. Rejects a missing comma or
 * any trailing junk ("1,2x") so a typo fails loudly rather than silently coordinating at 0.
 *
 * @return true on success; on failure @p aX / @p aY are left untouched.
 */
inline bool ParseXY( const std::string& aStr, double& aX, double& aY )
{
    std::size_t comma = aStr.find( ',' );

    if( comma == std::string::npos )
        return false;

    std::string xs = aStr.substr( 0, comma );
    std::string ys = aStr.substr( comma + 1 );

    try
    {
        std::size_t xUsed = 0, yUsed = 0;
        double      x = std::stod( xs, &xUsed );
        double      y = std::stod( ys, &yUsed );

        if( xUsed != xs.size() || yUsed != ys.size() )
            return false;

        aX = x;
        aY = y;
        return true;
    }
    catch( ... )
    {
        return false;
    }
}
} // namespace CLI

#endif
