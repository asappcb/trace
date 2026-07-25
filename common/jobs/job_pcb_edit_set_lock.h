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

#ifndef JOB_PCB_EDIT_SET_LOCK_H
#define JOB_PCB_EDIT_SET_LOCK_H

#include <kicommon.h>
#include "job.h"

/// Headless board edit: lock or unlock a footprint (by ref) or a net's tracks/vias (by net).
class KICOMMON_API JOB_PCB_EDIT_SET_LOCK : public JOB
{
public:
    JOB_PCB_EDIT_SET_LOCK();
    wxString GetDefaultDescription() const override;
    wxString GetSettingsDialogTitle() const override;

    wxString m_filename;
    wxString m_ref;           ///< footprint reference, or empty
    wxString m_net;           ///< net name, or empty
    bool     m_locked = true; ///< true = lock, false = unlock
};

#endif
