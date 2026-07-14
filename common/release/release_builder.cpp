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

#include <release/release_builder.h>

#include <algorithm>
#include <set>
#include <utility>

#include <wx/arrstr.h>
#include <wx/dir.h>
#include <wx/filename.h>


/// Path of @p aPath relative to @p aBaseDir, using '/' separators for cross-platform stability.
static wxString relPosix( const wxString& aPath, const wxString& aBaseDir )
{
    wxFileName fn( aPath );
    fn.MakeRelativeTo( aBaseDir );
    return fn.GetFullPath( wxPATH_UNIX );
}


/// Hash @p aFile and append it to @p aList with a path relative to @p aBaseDir. Silently skips a
/// file that cannot be read (a caller enumerating a live directory can race with deletion).
static void addFile( std::vector<RELEASE_FILE_ENTRY>& aList, const wxString& aFile,
                     const wxString& aBaseDir )
{
    bool     ok = false;
    uint64_t bytes = 0;
    wxString sha = ReleaseHashFile( aFile, &ok, &bytes );

    if( ok )
        aList.push_back( { relPosix( aFile, aBaseDir ), sha, bytes } );
}


bool BuildReleaseManifest( RELEASE_MANIFEST& aOut, const RELEASE_BUILD_OPTIONS& aOpts,
                           wxString* aError )
{
    wxFileName projectFn( aOpts.m_projectFile );

    if( !projectFn.FileExists() )
    {
        if( aError )
            *aError = wxString::Format( wxT( "Project file '%s' does not exist" ),
                                        aOpts.m_projectFile );

        return false;
    }

    if( !wxDirExists( aOpts.m_outputDir ) )
    {
        if( aError )
            *aError = wxString::Format( wxT( "Output directory '%s' does not exist" ),
                                        aOpts.m_outputDir );

        return false;
    }

    RELEASE_MANIFEST out;
    out.m_releaseName = aOpts.m_releaseName;
    out.m_createdUtc = aOpts.m_createdUtc;
    out.m_projectName = projectFn.GetName();
    out.m_kicadVersion = aOpts.m_kicadVersion;
    out.m_vcsHash = aOpts.m_vcsHash;
    out.m_vcsBranch = aOpts.m_vcsBranch;
    out.m_vcsDirty = aOpts.m_vcsDirty;

    // ---- design inputs: the project file, its sibling board, and every schematic sheet ----
    const wxString projectDir = projectFn.GetPath();

    std::vector<wxString> inputFiles;
    inputFiles.push_back( aOpts.m_projectFile );

    wxFileName boardFn( projectFn );
    boardFn.SetExt( wxT( "kicad_pcb" ) );

    if( boardFn.FileExists() )
        inputFiles.push_back( boardFn.GetFullPath() );

    // Top-level only (wxDIR_FILES): if the output dir lives inside the project dir, its contents
    // must not be swept up as schematic "inputs".
    wxArrayString schematics;
    wxDir::GetAllFiles( projectDir, &schematics, wxT( "*.kicad_sch" ), wxDIR_FILES );
    schematics.Sort();

    for( const wxString& sheet : schematics )
        inputFiles.push_back( sheet );

    // Absolute input paths, so the output walk can skip a design file that also sits under the
    // output directory (when outputs are written next to the project) instead of double-counting it.
    std::set<wxString> inputAbs;

    for( const wxString& f : inputFiles )
    {
        wxFileName fn( f );
        fn.MakeAbsolute();
        inputAbs.insert( fn.GetFullPath() );
        addFile( out.m_inputs, f, projectDir );
    }

    // ---- outputs: every file under the output directory (incl. hidden), minus any release
    //      manifest (this run's or a stale one) and minus files already counted as inputs ----
    wxArrayString outputs;
    wxDir::GetAllFiles( aOpts.m_outputDir, &outputs, wxEmptyString,
                        wxDIR_FILES | wxDIR_DIRS | wxDIR_HIDDEN );
    outputs.Sort();

    for( const wxString& file : outputs )
    {
        wxFileName fn( file );

        if( fn.GetExt().IsSameAs( wxT( "kicad_release" ), false ) )
            continue;

        wxFileName absFn( file );
        absFn.MakeAbsolute();

        if( inputAbs.count( absFn.GetFullPath() ) )
            continue;

        addFile( out.m_outputs, file, aOpts.m_outputDir );
    }

    aOut = std::move( out );
    return true;
}


std::vector<wxString> FindUnexpectedOutputs( const RELEASE_MANIFEST& aManifest,
                                             const wxString& aDir )
{
    std::set<wxString> recorded;

    for( const RELEASE_FILE_ENTRY& e : aManifest.m_outputs )
        recorded.insert( e.m_path );

    std::vector<wxString> extra;

    if( !wxDirExists( aDir ) )
        return extra;

    wxArrayString files;
    wxDir::GetAllFiles( aDir, &files, wxEmptyString, wxDIR_FILES | wxDIR_DIRS | wxDIR_HIDDEN );
    files.Sort();

    for( const wxString& f : files )
    {
        wxFileName fn( f );

        if( fn.GetExt().IsSameAs( wxT( "kicad_release" ), false ) )
            continue;

        wxString rel = relPosix( f, aDir );

        if( !recorded.count( rel ) )
            extra.push_back( rel );
    }

    return extra;
}
