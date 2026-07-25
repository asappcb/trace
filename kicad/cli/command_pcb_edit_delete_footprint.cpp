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

#include "command_pcb_edit_delete_footprint.h"

#include <cli/exit_codes.h>
#include <jobs/job_pcb_edit_delete_footprint.h>
#include <wx/crt.h>
#include <wx/file.h>

#include <string>

#define ARG_REF "--ref"

CLI::PCB_EDIT_DELETE_FOOTPRINT_COMMAND::PCB_EDIT_DELETE_FOOTPRINT_COMMAND() :
        COMMAND( "delete-footprint" )
{
    addCommonArgs( true, true, IO_TYPE::FILE, IO_TYPE::FILE );

    m_argParser.add_description( UTF8STDSTR( _( "Delete a footprint by reference and write the board back (overwrites "
                                                "the input when --output is omitted)" ) ) );

    m_argParser.add_argument( ARG_REF )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Reference of the footprint to delete (required)" ) ) )
            .metavar( "REF" );
}


int CLI::PCB_EDIT_DELETE_FOOTPRINT_COMMAND::doPerform( KIWAY& aKiway )
{
    std::unique_ptr<JOB_PCB_EDIT_DELETE_FOOTPRINT> job( new JOB_PCB_EDIT_DELETE_FOOTPRINT() );

    job->m_filename = m_argInput;
    job->SetConfiguredOutputPath( m_argOutput );
    job->m_ref = wxString::FromUTF8( m_argParser.get<std::string>( ARG_REF ).c_str() );

    if( !wxFile::Exists( job->m_filename ) )
    {
        wxFprintf( stderr, _( "Board file does not exist or is not accessible\n" ) );
        return CLI::EXIT_CODES::ERR_INVALID_INPUT_FILE;
    }

    if( job->m_ref.IsEmpty() )
    {
        wxFprintf( stderr, _( "--ref is required\n" ) );
        return CLI::EXIT_CODES::ERR_ARGS;
    }

    return aKiway.ProcessJob( KIWAY::FACE_PCB, job.get() );
}
