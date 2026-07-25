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

#include "command_pcb_edit_set_field.h"

#include <cli/exit_codes.h>
#include <jobs/job_pcb_edit_set_field.h>
#include <wx/crt.h>
#include <wx/file.h>

#include <string>
#include <vector>

#define ARG_REF "--ref"
#define ARG_VALUE "--value"
#define ARG_FIELD "--field"

CLI::PCB_EDIT_SET_FIELD_COMMAND::PCB_EDIT_SET_FIELD_COMMAND() :
        COMMAND( "set-field" )
{
    addCommonArgs( true, true, IO_TYPE::FILE, IO_TYPE::FILE );

    m_argParser.add_description( UTF8STDSTR( _( "Set a footprint's Value and/or named fields and write the board back "
                                                "(overwrites the input when --output is omitted)" ) ) );

    m_argParser.add_argument( ARG_REF )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Reference of the footprint to edit (required)" ) ) )
            .metavar( "REF" );

    m_argParser.add_argument( ARG_VALUE )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "New Value field" ) ) )
            .metavar( "VALUE" );

    m_argParser.add_argument( ARG_FIELD )
            .default_value( std::vector<std::string>() )
            .append()
            .help( UTF8STDSTR( _( "Set a named field as NAME=VALUE; repeatable" ) ) )
            .metavar( "NAME=VALUE" );
}


int CLI::PCB_EDIT_SET_FIELD_COMMAND::doPerform( KIWAY& aKiway )
{
    std::unique_ptr<JOB_PCB_EDIT_SET_FIELD> job( new JOB_PCB_EDIT_SET_FIELD() );

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

    std::string value = m_argParser.get<std::string>( ARG_VALUE );

    if( m_argParser.is_used( ARG_VALUE ) )
    {
        job->m_hasValue = true;
        job->m_value = wxString::FromUTF8( value.c_str() );
    }

    for( const std::string& spec : m_argParser.get<std::vector<std::string>>( ARG_FIELD ) )
    {
        std::size_t eq = spec.find( '=' );

        if( eq == std::string::npos || eq == 0 )
        {
            wxFprintf( stderr, _( "--field must be given as NAME=VALUE\n" ) );
            return CLI::EXIT_CODES::ERR_ARGS;
        }

        job->m_fields.emplace_back( wxString::FromUTF8( spec.substr( 0, eq ).c_str() ),
                                    wxString::FromUTF8( spec.substr( eq + 1 ).c_str() ) );
    }

    if( !job->m_hasValue && job->m_fields.empty() )
    {
        wxFprintf( stderr, _( "Nothing to do: pass --value and/or --field NAME=VALUE\n" ) );
        return CLI::EXIT_CODES::ERR_ARGS;
    }

    return aKiway.ProcessJob( KIWAY::FACE_PCB, job.get() );
}
