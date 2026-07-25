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

#include "command_pcb_edit_cleanup_tracks.h"

#include <cli/exit_codes.h>
#include <jobs/job_pcb_edit_cleanup_tracks.h>
#include <wx/crt.h>
#include <wx/file.h>

#include <string>

#define ARG_NET "--net"

CLI::PCB_EDIT_CLEANUP_TRACKS_COMMAND::PCB_EDIT_CLEANUP_TRACKS_COMMAND() :
        COMMAND( "cleanup-tracks" )
{
    addCommonArgs( true, true, IO_TYPE::FILE, IO_TYPE::FILE );

    m_argParser.add_description( UTF8STDSTR( _( "Clean up tracks (merge collinear/redundant segments, remove dangling "
                                                "tracks and vias, tracks inside pads, short circuits) and write the "
                                                "board back (overwrites the input when --output is omitted)" ) ) );

    m_argParser.add_argument( ARG_NET )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Clean only this net; if omitted, the whole board is cleaned" ) ) )
            .metavar( "NET" );
}


int CLI::PCB_EDIT_CLEANUP_TRACKS_COMMAND::doPerform( KIWAY& aKiway )
{
    std::unique_ptr<JOB_PCB_EDIT_CLEANUP_TRACKS> job( new JOB_PCB_EDIT_CLEANUP_TRACKS() );

    job->m_filename = m_argInput;
    job->SetConfiguredOutputPath( m_argOutput );
    job->m_net = wxString::FromUTF8( m_argParser.get<std::string>( ARG_NET ).c_str() );

    if( !wxFile::Exists( job->m_filename ) )
    {
        wxFprintf( stderr, _( "Board file does not exist or is not accessible\n" ) );
        return CLI::EXIT_CODES::ERR_INVALID_INPUT_FILE;
    }

    return aKiway.ProcessJob( KIWAY::FACE_PCB, job.get() );
}
