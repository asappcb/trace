/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef BACKDRILL_VIA_EDIT_H
#define BACKDRILL_VIA_EDIT_H

#include <optional>

#include <layer_ids.h>
#include <padstack.h>

/**
 * Apply a via-properties-dialog backdrill edit to a shared padstack.
 *
 * Factored out of DIALOG_TRACK_VIA_PROPERTIES so the multi-selection decision is unit-testable
 * (the dialog itself has no test harness). It routes through the PADSTACK setter API
 * (PADSTACK::backdrillWriteSlot), the same strategy the pad dialog uses, so both editors preserve a
 * KiCad 10.0 slot layout in place rather than canonicalizing it.
 *
 * @param aSharedStack the shared editing padstack, seeded from the first selected via and copied
 *                     onto every via that is flagged for update. Mutated in place.
 * @param aVia         the padstack of the via currently being processed (compared against, not
 *                     modified).
 * @param aMode        the chosen backdrill mode, or std::nullopt when the mode control is
 *                     indeterminate across a multi-selection (the vias disagree on which sides are
 *                     backdrilled). When indeterminate, the set of backdrilled sides is left alone
 *                     and only an explicit size change is pushed.
 * @param aFrontSize   the front (top) backdrill size, or std::nullopt when the field is
 *                     blank/indeterminate.
 * @param aBackSize    the back (bottom) backdrill size, or std::nullopt when blank/indeterminate.
 * @param aFrontEnd    the front must-cut layer, or UNDEFINED_LAYER to leave it unchanged.
 * @param aBackEnd     the back must-cut layer, or UNDEFINED_LAYER to leave it unchanged.
 *
 * @return true if @p aVia differs from the (possibly mutated) @p aSharedStack and must therefore be
 *         updated with it. When @p aMode is std::nullopt this is true only when an explicit size
 *         edit actually changed an existing backdrill, so a blank field on a mixed selection never
 *         drags the shared stack's backdrill onto vias the user did not touch.
 */
inline bool ApplyViaBackdrillEdit( PADSTACK& aSharedStack, const PADSTACK& aVia,
                                   std::optional<BACKDRILL_MODE> aMode,
                                   std::optional<int> aFrontSize, std::optional<int> aBackSize,
                                   PCB_LAYER_ID aFrontEnd, PCB_LAYER_ID aBackEnd )
{
    bool updatePadstack = false;

    if( aMode.has_value() )
    {
        // Establishes which sides carry a backdrill (adding/removing slots and seeding a default
        // size for a freshly added side).
        aSharedStack.SetBackdrillMode( *aMode );

        const bool wantTop = *aMode == BACKDRILL_MODE::BACKDRILL_TOP
                             || *aMode == BACKDRILL_MODE::BACKDRILL_BOTH;
        const bool wantBottom = *aMode == BACKDRILL_MODE::BACKDRILL_BOTTOM
                                || *aMode == BACKDRILL_MODE::BACKDRILL_BOTH;

        if( wantTop )
        {
            if( aFrontSize.has_value() )
                aSharedStack.SetBackdrillSize( true, *aFrontSize );

            if( aFrontEnd != UNDEFINED_LAYER )
                aSharedStack.SetBackdrillEndLayer( true, aFrontEnd );
        }

        if( wantBottom )
        {
            if( aBackSize.has_value() )
                aSharedStack.SetBackdrillSize( false, *aBackSize );

            if( aBackEnd != UNDEFINED_LAYER )
                aSharedStack.SetBackdrillEndLayer( false, aBackEnd );
        }

        // A mode was explicitly chosen: propagate the edited backdrill to every selected via whose
        // backdrill differs. Compare against this via's own padstack, not a snapshot of the shared
        // stack (which persists and accumulates across the multi-selection loop).
        if( aSharedStack.SecondaryDrill() != aVia.SecondaryDrill()
                || aSharedStack.TertiaryDrill() != aVia.TertiaryDrill() )
        {
            updatePadstack = true;
        }
    }
    else
    {
        // Mode is indeterminate across the selection: leave each via's set of sides alone and only
        // push an explicit size change to a side that already exists on the shared stack. A blank or
        // unchanged field must NOT drag the shared stack's backdrill onto vias the user did not edit.
        if( aFrontSize.has_value() && aSharedStack.GetBackdrillSize( true ).has_value()
                && aSharedStack.GetBackdrillSize( true ) != aFrontSize )
        {
            aSharedStack.SetBackdrillSize( true, *aFrontSize );
            updatePadstack = true;
        }

        if( aBackSize.has_value() && aSharedStack.GetBackdrillSize( false ).has_value()
                && aSharedStack.GetBackdrillSize( false ) != aBackSize )
        {
            aSharedStack.SetBackdrillSize( false, *aBackSize );
            updatePadstack = true;
        }
    }

    return updatePadstack;
}

#endif // BACKDRILL_VIA_EDIT_H
