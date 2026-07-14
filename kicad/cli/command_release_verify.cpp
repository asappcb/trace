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

#include "command_release_verify.h"
#include <cli/exit_codes.h>
#include <string_utils.h>
#include <reporter.h>

#include <wx/crt.h>
#include <wx/filename.h>

#include <release/release_builder.h>
#include <release/release_manifest.h>

#define ARG_DIR "--dir"


CLI::RELEASE_VERIFY_COMMAND::RELEASE_VERIFY_COMMAND() : COMMAND( "verify" )
{
    // Input = the .kicad_release manifest to check.
    addCommonArgs( true, false, IO_TYPE::FILE, IO_TYPE::FILE );

    m_argParser.add_description( UTF8STDSTR( _(
            "Verify a release: recompute output hashes and report any drift from the manifest" ) ) );

    m_argParser.add_argument( ARG_DIR )
            .help( UTF8STDSTR(
                    _( "Directory of outputs to check (default: the manifest's directory)" ) ) )
            .default_value( std::string( "" ) )
            .metavar( "DIR" );
}


int CLI::RELEASE_VERIFY_COMMAND::doPerform( KIWAY& aKiway )
{
    wxFileName manifestFn( m_argInput );
    manifestFn.MakeAbsolute();

    if( !manifestFn.FileExists() )
    {
        wxFprintf( stderr, _( "Release manifest does not exist or is not accessible\n" ) );
        return EXIT_CODES::ERR_INVALID_INPUT_FILE;
    }

    RELEASE_MANIFEST manifest;
    wxString         err;

    if( !RELEASE_MANIFEST::LoadFromFile( manifestFn.GetFullPath(), manifest, &err ) )
    {
        wxFprintf( stderr, wxT( "%s\n" ), err );
        return EXIT_CODES::ERR_INVALID_INPUT_FILE;
    }

    wxString dir = From_UTF8( m_argParser.get<std::string>( ARG_DIR ).c_str() );

    if( dir.IsEmpty() )
        dir = manifestFn.GetPath();

    RELEASE_MANIFEST::VERIFY_RESULT result = manifest.VerifyOutputs( dir );

    REPORTER& reporter = CLI_REPORTER::GetInstance();

    if( !result.m_contentHashOk )
    {
        reporter.Report( _( "Manifest content hash does not match its entries (tampered manifest)" ),
                         RPT_SEVERITY_ERROR );
    }

    int changed = 0, missing = 0, unreadable = 0;

    for( const RELEASE_MANIFEST::VERIFY_ENTRY& e : result.m_entries )
    {
        switch( e.m_status )
        {
        case RELEASE_MANIFEST::VERIFY_ENTRY::OK: break;

        case RELEASE_MANIFEST::VERIFY_ENTRY::CHANGED:
            ++changed;
            reporter.Report( wxString::Format( _( "CHANGED: %s" ), e.m_path ), RPT_SEVERITY_ERROR );
            break;

        case RELEASE_MANIFEST::VERIFY_ENTRY::MISSING:
            ++missing;
            reporter.Report( wxString::Format( _( "MISSING: %s" ), e.m_path ), RPT_SEVERITY_ERROR );
            break;

        case RELEASE_MANIFEST::VERIFY_ENTRY::UNREADABLE:
            ++unreadable;
            reporter.Report( wxString::Format( _( "UNREADABLE: %s" ), e.m_path ),
                             RPT_SEVERITY_ERROR );
            break;
        }
    }

    // Files present in the directory but not recorded in the manifest cannot make a recorded
    // output drift, but they are still worth surfacing (an added artifact). Report as warnings;
    // they do not by themselves fail verification.
    for( const wxString& extra : FindUnexpectedOutputs( manifest, dir ) )
        reporter.Report( wxString::Format( _( "UNEXPECTED: %s" ), extra ), RPT_SEVERITY_WARNING );

    if( result.Ok() )
    {
        reporter.Report( wxString::Format( _( "Release verified: %zu output(s) match" ),
                                           result.m_entries.size() ),
                         RPT_SEVERITY_INFO );
        return EXIT_CODES::SUCCESS;
    }

    reporter.Report( wxString::Format( _( "Release verification failed: %d changed, %d missing, "
                                          "%d unreadable" ),
                                       changed, missing, unreadable ),
                     RPT_SEVERITY_ERROR );

    return EXIT_CODES::ERR_RC_VIOLATIONS;
}
