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

#include "command_pcb_edit_set_track_width.h"

#include <cli/exit_codes.h>
#include <jobs/job_pcb_edit_set_track_width.h>
#include <wx/crt.h>
#include <wx/file.h>

#define ARG_WIDTH "--width"
#define ARG_NET "--net"

CLI::PCB_EDIT_SET_TRACK_WIDTH_COMMAND::PCB_EDIT_SET_TRACK_WIDTH_COMMAND() :
        COMMAND( "set-track-width" )
{
    // --output optional: when omitted the edited board is written back over the input.
    addCommonArgs( true, true, IO_TYPE::FILE, IO_TYPE::FILE );

    m_argParser.add_description(
            UTF8STDSTR( _( "Set the width of copper tracks (optionally on one net) and write the "
                           "board back (overwrites the input when --output is omitted)" ) ) );

    m_argParser.add_argument( ARG_WIDTH )
            .default_value( 0.0 )
            .scan<'g', double>()
            .help( UTF8STDSTR( _( "New track width in millimetres (required, > 0)" ) ) )
            .metavar( "WIDTH_MM" );

    m_argParser.add_argument( ARG_NET )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Restrict the change to tracks on this net; if omitted, every "
                                  "track is changed" ) ) )
            .metavar( "NET" );
}


int CLI::PCB_EDIT_SET_TRACK_WIDTH_COMMAND::doPerform( KIWAY& aKiway )
{
    std::unique_ptr<JOB_PCB_EDIT_SET_TRACK_WIDTH> job( new JOB_PCB_EDIT_SET_TRACK_WIDTH() );

    job->m_filename = m_argInput;
    job->SetConfiguredOutputPath( m_argOutput );
    job->m_widthMM = m_argParser.get<double>( ARG_WIDTH );
    job->m_net = wxString::FromUTF8( m_argParser.get<std::string>( ARG_NET ).c_str() );

    if( !wxFile::Exists( job->m_filename ) )
    {
        wxFprintf( stderr, _( "Board file does not exist or is not accessible\n" ) );
        return CLI::EXIT_CODES::ERR_INVALID_INPUT_FILE;
    }

    if( job->m_widthMM <= 0.0 )
    {
        wxFprintf( stderr, _( "--width must be a positive number of millimetres\n" ) );
        return CLI::EXIT_CODES::ERR_ARGS;
    }

    return aKiway.ProcessJob( KIWAY::FACE_PCB, job.get() );
}
