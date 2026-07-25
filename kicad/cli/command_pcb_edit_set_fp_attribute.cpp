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

#include "command_pcb_edit_set_fp_attribute.h"

#include <cli/exit_codes.h>
#include <cli/cli_bool.h>
#include <jobs/job_pcb_edit_set_fp_attribute.h>
#include <wx/crt.h>
#include <wx/file.h>

#include <string>

#define ARG_REF "--ref"
#define ARG_DNP "--dnp"
#define ARG_EXCLUDE_BOM "--exclude-from-bom"
#define ARG_EXCLUDE_POS "--exclude-from-pos"

CLI::PCB_EDIT_SET_FP_ATTRIBUTE_COMMAND::PCB_EDIT_SET_FP_ATTRIBUTE_COMMAND() :
        COMMAND( "set-footprint-attribute" )
{
    addCommonArgs( true, true, IO_TYPE::FILE, IO_TYPE::FILE );

    m_argParser.add_description( UTF8STDSTR( _( "Set assembly/fab attributes on a footprint and write the board back "
                                                "(overwrites the input when --output is omitted)" ) ) );

    m_argParser.add_argument( ARG_REF )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Reference of the footprint to edit (required)" ) ) )
            .metavar( "REF" );

    m_argParser.add_argument( ARG_DNP )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Mark Do-Not-Populate: yes|no" ) ) )
            .metavar( "yes|no" );

    m_argParser.add_argument( ARG_EXCLUDE_BOM )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Exclude from BOM: yes|no" ) ) )
            .metavar( "yes|no" );

    m_argParser.add_argument( ARG_EXCLUDE_POS )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Exclude from position files: yes|no" ) ) )
            .metavar( "yes|no" );
}


int CLI::PCB_EDIT_SET_FP_ATTRIBUTE_COMMAND::doPerform( KIWAY& aKiway )
{
    std::unique_ptr<JOB_PCB_EDIT_SET_FP_ATTRIBUTE> job( new JOB_PCB_EDIT_SET_FP_ATTRIBUTE() );

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

    // Each attribute is optional; a yes/no value sets or clears it, absence leaves it unchanged.
    auto readTri = [&]( const char* aArg, int& aOut ) -> bool
    {
        std::string v = m_argParser.get<std::string>( aArg );

        if( v.empty() )
            return true;

        bool b = false;

        if( !CLI::ParseYesNo( v, b ) )
        {
            wxFprintf( stderr, _( "%s must be yes or no\n" ), aArg );
            return false;
        }

        aOut = b ? 1 : 0;
        return true;
    };

    if( !readTri( ARG_DNP, job->m_dnp ) || !readTri( ARG_EXCLUDE_BOM, job->m_excludeFromBOM )
        || !readTri( ARG_EXCLUDE_POS, job->m_excludeFromPos ) )
    {
        return CLI::EXIT_CODES::ERR_ARGS;
    }

    if( job->m_dnp < 0 && job->m_excludeFromBOM < 0 && job->m_excludeFromPos < 0 )
    {
        wxFprintf( stderr, _( "Nothing to do: pass at least one of --dnp, --exclude-from-bom, "
                              "--exclude-from-pos\n" ) );
        return CLI::EXIT_CODES::ERR_ARGS;
    }

    return aKiway.ProcessJob( KIWAY::FACE_PCB, job.get() );
}
