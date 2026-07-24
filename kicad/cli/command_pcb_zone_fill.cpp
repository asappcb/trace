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

#include "command_pcb_zone_fill.h"

#include <cli/exit_codes.h>
#include <jobs/job_pcb_fill_zones.h>
#include <string_utils.h>
#include <wx/crt.h>
#include <wx/file.h>

CLI::PCB_ZONE_FILL_COMMAND::PCB_ZONE_FILL_COMMAND() : COMMAND( "fill" )
{
    // --output optional: when omitted the filled board is written back over the input, which is
    // the point of the command ("fill my board").
    addCommonArgs( true, true, IO_TYPE::FILE, IO_TYPE::FILE );

    m_argParser.add_description(
            UTF8STDSTR( _( "Fill all copper zones and write the board back (overwrites the input "
                           "when --output is omitted)" ) ) );
}


int CLI::PCB_ZONE_FILL_COMMAND::doPerform( KIWAY& aKiway )
{
    std::unique_ptr<JOB_PCB_FILL_ZONES> fillJob( new JOB_PCB_FILL_ZONES() );

    fillJob->m_filename = m_argInput;
    fillJob->SetConfiguredOutputPath( m_argOutput );

    if( !wxFile::Exists( fillJob->m_filename ) )
    {
        wxFprintf( stderr, _( "Board file does not exist or is not accessible\n" ) );
        return CLI::EXIT_CODES::ERR_INVALID_INPUT_FILE;
    }

    return aKiway.ProcessJob( KIWAY::FACE_PCB, fillJob.get() );
}
