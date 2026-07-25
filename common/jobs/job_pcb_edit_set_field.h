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

#ifndef JOB_PCB_EDIT_SET_FIELD_H
#define JOB_PCB_EDIT_SET_FIELD_H

#include <kicommon.h>
#include "job.h"

#include <utility>
#include <vector>

/// Headless board edit: set a footprint's Value and/or named fields (properties) by reference.
class KICOMMON_API JOB_PCB_EDIT_SET_FIELD : public JOB
{
public:
    JOB_PCB_EDIT_SET_FIELD();
    wxString GetDefaultDescription() const override;
    wxString GetSettingsDialogTitle() const override;

    wxString                                   m_filename;
    wxString                                   m_ref; ///< footprint reference (required)
    bool                                       m_hasValue = false;
    wxString                                   m_value;  ///< new Value when m_hasValue
    std::vector<std::pair<wxString, wxString>> m_fields; ///< name -> value pairs to set
};

#endif
