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

#ifndef JOB_PCB_EDIT_CLEANUP_TRACKS_H
#define JOB_PCB_EDIT_CLEANUP_TRACKS_H

#include <kicommon.h>
#include "job.h"

/// Headless board edit: run KiCad's track cleaner (merge collinear/redundant segments, drop
/// dangling tracks & vias, tracks-in-pads, short circuits) and write the board back.
class KICOMMON_API JOB_PCB_EDIT_CLEANUP_TRACKS : public JOB
{
public:
    JOB_PCB_EDIT_CLEANUP_TRACKS();
    wxString GetDefaultDescription() const override;
    wxString GetSettingsDialogTitle() const override;

    wxString m_filename;
    wxString m_net; ///< optional: clean only this net; empty => the whole board
};

#endif
