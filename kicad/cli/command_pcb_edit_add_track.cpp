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

#include "command_pcb_edit_add_track.h"

#include <cli/exit_codes.h>
#include <cli/cli_xy.h>
#include <jobs/job_pcb_edit_add_track.h>
#include <wx/crt.h>
#include <wx/file.h>

#include <string>

#define ARG_NET "--net"
#define ARG_START "--start"
#define ARG_END "--end"
#define ARG_LAYER "--layer"
#define ARG_WIDTH "--width"

CLI::PCB_EDIT_ADD_TRACK_COMMAND::PCB_EDIT_ADD_TRACK_COMMAND() :
        COMMAND( "add-track" )
{
    addCommonArgs( true, true, IO_TYPE::FILE, IO_TYPE::FILE );

    m_argParser.add_description( UTF8STDSTR( _( "Add a straight copper track segment on a net and write the board back "
                                                "(overwrites the input when --output is omitted)" ) ) );

    m_argParser.add_argument( ARG_NET )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Net the track belongs to (required)" ) ) )
            .metavar( "NET" );

    m_argParser.add_argument( ARG_START )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Start point as X,Y in millimetres (required)" ) ) )
            .metavar( "X,Y" );

    m_argParser.add_argument( ARG_END )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "End point as X,Y in millimetres (required)" ) ) )
            .metavar( "X,Y" );

    m_argParser.add_argument( ARG_LAYER )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Copper layer name (required), e.g. F.Cu" ) ) )
            .metavar( "LAYER" );

    m_argParser.add_argument( ARG_WIDTH )
            .default_value( 0.0 )
            .scan<'g', double>()
            .help( UTF8STDSTR( _( "Track width in mm; defaults to the net's netclass" ) ) )
            .metavar( "WIDTH_MM" );
}


int CLI::PCB_EDIT_ADD_TRACK_COMMAND::doPerform( KIWAY& aKiway )
{
    std::unique_ptr<JOB_PCB_EDIT_ADD_TRACK> job( new JOB_PCB_EDIT_ADD_TRACK() );

    job->m_filename = m_argInput;
    job->SetConfiguredOutputPath( m_argOutput );
    job->m_net = wxString::FromUTF8( m_argParser.get<std::string>( ARG_NET ).c_str() );
    job->m_layer = wxString::FromUTF8( m_argParser.get<std::string>( ARG_LAYER ).c_str() );
    job->m_widthMM = m_argParser.get<double>( ARG_WIDTH );

    if( !wxFile::Exists( job->m_filename ) )
    {
        wxFprintf( stderr, _( "Board file does not exist or is not accessible\n" ) );
        return CLI::EXIT_CODES::ERR_INVALID_INPUT_FILE;
    }

    if( job->m_net.IsEmpty() || job->m_layer.IsEmpty() )
    {
        wxFprintf( stderr, _( "--net and --layer are required\n" ) );
        return CLI::EXIT_CODES::ERR_ARGS;
    }

    if( !CLI::ParseXY( m_argParser.get<std::string>( ARG_START ), job->m_startX, job->m_startY )
        || !CLI::ParseXY( m_argParser.get<std::string>( ARG_END ), job->m_endX, job->m_endY ) )
    {
        wxFprintf( stderr, _( "--start and --end must each be X,Y in millimetres, e.g. 10,10\n" ) );
        return CLI::EXIT_CODES::ERR_ARGS;
    }

    return aKiway.ProcessJob( KIWAY::FACE_PCB, job.get() );
}
