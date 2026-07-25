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

#ifndef JOB_PCB_EDIT_SET_VIA_SIZE_H
#define JOB_PCB_EDIT_SET_VIA_SIZE_H

#include <kicommon.h>
#include "job.h"

/// Headless board edit: set via diameter and/or drill, optionally restricted to one net.
class KICOMMON_API JOB_PCB_EDIT_SET_VIA_SIZE : public JOB
{
public:
    JOB_PCB_EDIT_SET_VIA_SIZE();
    wxString GetDefaultDescription() const override;
    wxString GetSettingsDialogTitle() const override;

    wxString m_filename;
    wxString m_net;           ///< optional; empty => every via
    double   m_sizeMM = 0.0;  ///< <= 0 => leave diameter unchanged
    double   m_drillMM = 0.0; ///< <= 0 => leave drill unchanged
};

#endif
