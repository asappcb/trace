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

#include "command_release_create.h"
#include <cli/exit_codes.h>
#include <string_utils.h>
#include <reporter.h>

#include <wx/crt.h>
#include <wx/datetime.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <build_version.h>
#include <gestfich.h>
#include <git/project_git_utils.h>
#include <release/release_builder.h>
#include <release/release_manifest.h>

#define ARG_VERSION "--version"
#define ARG_ZIP "--zip"


CLI::RELEASE_CREATE_COMMAND::RELEASE_CREATE_COMMAND() : COMMAND( "create" )
{
    // Input = project file; output = directory of produced outputs to fingerprint.
    addCommonArgs( true, true, IO_TYPE::FILE, IO_TYPE::DIRECTORY );

    m_argParser.add_description( UTF8STDSTR( _(
            "Stamp a versioned, revision-bound release manifest over a directory of design "
            "outputs" ) ) );

    m_argParser.add_argument( ARG_VERSION )
            .help( UTF8STDSTR( _( "Release version/tag; also names the manifest file" ) ) )
            .default_value( std::string( "" ) )
            .metavar( "VERSION" );

    m_argParser.add_argument( ARG_ZIP )
            .help( UTF8STDSTR( _( "Also bundle the output directory into <output>.zip" ) ) )
            .flag();
}


int CLI::RELEASE_CREATE_COMMAND::doPerform( KIWAY& aKiway )
{
    wxFileName projectFn( m_argInput );
    projectFn.MakeAbsolute();
    wxString projectFile = projectFn.GetFullPath();

    if( !projectFn.FileExists() )
    {
        wxFprintf( stderr, _( "Project file does not exist or is not accessible\n" ) );
        return EXIT_CODES::ERR_INVALID_INPUT_FILE;
    }

    if( m_argOutput.IsEmpty() || !wxDirExists( m_argOutput ) )
    {
        wxFprintf( stderr, _( "An existing output directory is required (--output)\n" ) );
        return EXIT_CODES::ERR_ARGS;
    }

    // Normalize to an absolute directory so relative paths and the --zip name are well defined.
    wxFileName outputDirFn = wxFileName::DirName( m_argOutput );
    outputDirFn.MakeAbsolute();
    wxString   outputDir = outputDirFn.GetPath();

    wxString version = From_UTF8( m_argParser.get<std::string>( ARG_VERSION ).c_str() );

    // Manifest file name derives from the version, or the project name if no version was given.
    // Keep it filesystem-safe.
    wxString manifestBase = version.IsEmpty() ? projectFn.GetName() : version;
    manifestBase.Replace( wxT( "/" ), wxT( "_" ) );
    manifestBase.Replace( wxT( "\\" ), wxT( "_" ) );
    manifestBase.Replace( wxT( " " ), wxT( "_" ) );
    wxString manifestName = manifestBase + wxT( ".kicad_release" );

    RELEASE_BUILD_OPTIONS opts;
    opts.m_projectFile = projectFile;
    opts.m_outputDir = outputDir;
    opts.m_releaseName = version;
    opts.m_createdUtc = wxDateTime::Now().ToUTC().FormatISOCombined( 'T' ) + wxT( "Z" );
    opts.m_kicadVersion = GetSemanticVersion();
    opts.m_vcsHash = KIGIT::PROJECT_GIT_UTILS::GetCurrentHash( projectFile, false );
    opts.m_vcsBranch = KIGIT::PROJECT_GIT_UTILS::GetCurrentBranch( projectFile );
    opts.m_vcsDirty = KIGIT::PROJECT_GIT_UTILS::HasUncommittedChanges( projectFile );

    RELEASE_MANIFEST manifest;
    wxString         err;

    if( !BuildReleaseManifest( manifest, opts, &err ) )
    {
        wxFprintf( stderr, wxT( "%s\n" ), err );
        return EXIT_CODES::ERR_UNKNOWN;
    }

    wxFileName manifestFn( outputDir, manifestName );

    if( !manifest.WriteToFile( manifestFn.GetFullPath(), &err ) )
    {
        wxFprintf( stderr, wxT( "%s\n" ), err );
        return EXIT_CODES::ERR_UNKNOWN;
    }

    REPORTER& reporter = CLI_REPORTER::GetInstance();
    reporter.Report( wxString::Format(
                             _( "Release '%s': %zu input(s), %zu output(s), revision %s; wrote %s" ),
                             manifestBase, manifest.m_inputs.size(), manifest.m_outputs.size(),
                             opts.m_vcsHash, manifestFn.GetFullPath() ),
                     RPT_SEVERITY_INFO );

    if( m_argParser.get<bool>( ARG_ZIP ) )
    {
        wxString zipPath = outputDir;

        while( !zipPath.IsEmpty() && wxFileName::IsPathSeparator( zipPath.Last() ) )
            zipPath.RemoveLast();

        zipPath += wxT( ".zip" );

        wxFFileOutputStream fileStream( zipPath );

        if( !fileStream.IsOk() )
        {
            wxFprintf( stderr, _( "Cannot open '%s' for writing\n" ), zipPath );
            return EXIT_CODES::ERR_UNKNOWN;
        }

        wxZipOutputStream zip( fileStream );
        wxString          zipErrors;

        if( !AddDirectoryToZip( zip, outputDir, zipErrors ) )
        {
            wxFprintf( stderr, wxT( "%s\n" ), zipErrors );
            return EXIT_CODES::ERR_UNKNOWN;
        }

        zip.Close();
        fileStream.Close();

        reporter.Report( wxString::Format( _( "Bundled release into %s" ), zipPath ),
                         RPT_SEVERITY_INFO );
    }

    return EXIT_CODES::SUCCESS;
}
