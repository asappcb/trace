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

#include <jobs/job_pcb_edit_set_track_width.h>
#include <i18n_utility.h>

JOB_PCB_EDIT_SET_TRACK_WIDTH::JOB_PCB_EDIT_SET_TRACK_WIDTH() :
        JOB( "edit_set_track_width", false ),
        m_filename()
{
}


wxString JOB_PCB_EDIT_SET_TRACK_WIDTH::GetDefaultDescription() const
{
    return _( "Set track width" );
}


wxString JOB_PCB_EDIT_SET_TRACK_WIDTH::GetSettingsDialogTitle() const
{
    return _( "Set Track Width Job Settings" );
}
