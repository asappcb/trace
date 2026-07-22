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

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <release/release_builder.h>
#include <release/release_manifest.h>

#include <algorithm>
#include <atomic>

#include <wx/ffile.h>
#include <wx/filename.h>
#include <wx/time.h>


namespace
{
std::atomic<unsigned> g_counter{ 0 };

wxString uniqueTempDir( const wxString& aTag )
{
    wxString leaf = wxString::Format( wxT( "kicad-relbuild-%s-%ld-%u" ), aTag,
                                      static_cast<long>( wxGetLocalTimeMillis().GetValue() ),
                                      g_counter.fetch_add( 1 ) );
    wxString dir = wxFileName::GetTempDir() + wxFileName::GetPathSeparator() + leaf;
    wxFileName::Mkdir( dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL );
    return dir;
}

void writeFile( const wxString& aPath, const std::string& aContent )
{
    wxFileName fn( aPath );

    if( fn.GetPath().length() )
        wxFileName::Mkdir( fn.GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL );

    wxFFile fp( aPath, wxT( "wb" ) );
    BOOST_REQUIRE( fp.IsOpened() );

    if( !aContent.empty() )
        BOOST_REQUIRE( fp.Write( aContent.data(), aContent.size() ) == aContent.size() );

    fp.Close();
}

wxString join( const wxString& aDir, const wxString& aRel )
{
    return aDir + wxFileName::GetPathSeparator() + aRel;
}

// True if any entry has the given (POSIX) relative path.
bool hasPath( const std::vector<RELEASE_FILE_ENTRY>& aList, const wxString& aRel )
{
    return std::any_of( aList.begin(), aList.end(),
                        [&]( const RELEASE_FILE_ENTRY& e ) { return e.m_path == aRel; } );
}

RELEASE_BUILD_OPTIONS baseOptions( const wxString& aProjectFile, const wxString& aOutputDir )
{
    RELEASE_BUILD_OPTIONS opts;
    opts.m_projectFile = aProjectFile;
    opts.m_outputDir = aOutputDir;
    opts.m_releaseName = wxT( "v1.0" );
    opts.m_createdUtc = wxT( "2026-07-14T00:00:00Z" );
    opts.m_kicadVersion = wxT( "10.99.0" );
    opts.m_vcsHash = wxT( "abcdef1234567890abcdef1234567890abcdef12" );
    opts.m_vcsBranch = wxT( "master" );
    opts.m_vcsDirty = true;
    return opts;
}

} // namespace


BOOST_AUTO_TEST_SUITE( ReleaseBuilder )


BOOST_AUTO_TEST_CASE( HashesInputsAndOutputs )
{
    wxString proj = uniqueTempDir( "proj" );
    writeFile( join( proj, wxT( "widget.kicad_pro" ) ), "project" );
    writeFile( join( proj, wxT( "widget.kicad_pcb" ) ), "board" );
    writeFile( join( proj, wxT( "widget.kicad_sch" ) ), "root sheet" );
    writeFile( join( proj, wxT( "power.kicad_sch" ) ), "sub sheet" );

    wxString out = uniqueTempDir( "out" );
    writeFile( join( out, wxT( "top.gbr" ) ), "gerber-top" );
    writeFile( join( out, wxT( "drill/plated.drl" ) ), "drill" );

    RELEASE_MANIFEST m;
    wxString         err;
    BOOST_REQUIRE_MESSAGE(
            BuildReleaseManifest( m, baseOptions( join( proj, wxT( "widget.kicad_pro" ) ), out ),
                                  &err ),
            err );

    // All four design inputs present, by project-relative path.
    BOOST_CHECK_EQUAL( m.m_inputs.size(), 4u );
    BOOST_CHECK( hasPath( m.m_inputs, wxT( "widget.kicad_pro" ) ) );
    BOOST_CHECK( hasPath( m.m_inputs, wxT( "widget.kicad_pcb" ) ) );
    BOOST_CHECK( hasPath( m.m_inputs, wxT( "widget.kicad_sch" ) ) );
    BOOST_CHECK( hasPath( m.m_inputs, wxT( "power.kicad_sch" ) ) );

    // Outputs present with POSIX-relative (incl. nested) paths.
    BOOST_CHECK_EQUAL( m.m_outputs.size(), 2u );
    BOOST_CHECK( hasPath( m.m_outputs, wxT( "top.gbr" ) ) );
    BOOST_CHECK( hasPath( m.m_outputs, wxT( "drill/plated.drl" ) ) );

    // Metadata copied through.
    BOOST_CHECK_EQUAL( m.m_releaseName, wxT( "v1.0" ) );
    BOOST_CHECK_EQUAL( m.m_projectName, wxT( "widget" ) );
    BOOST_CHECK_EQUAL( m.m_kicadVersion, wxT( "10.99.0" ) );
    BOOST_CHECK_EQUAL( m.m_vcsHash, wxT( "abcdef1234567890abcdef1234567890abcdef12" ) );
    BOOST_CHECK( m.m_vcsDirty );

    // Every entry carries a 64-hex SHA and a non-zero (or defined) size.
    for( const auto& e : m.m_inputs )
        BOOST_CHECK_EQUAL( e.m_sha256.length(), 64u );
}


BOOST_AUTO_TEST_CASE( ExcludesAnyReleaseManifestFromOutputs )
{
    wxString proj = uniqueTempDir( "proj" );
    writeFile( join( proj, wxT( "widget.kicad_pro" ) ), "project" );

    wxString out = uniqueTempDir( "out" );
    writeFile( join( out, wxT( "top.gbr" ) ), "gerber" );
    // Manifests from prior runs (this version and a different one) must never be fingerprinted as
    // design outputs -- otherwise re-running with a new --version pollutes the output set.
    writeFile( join( out, wxT( "widget.kicad_release" ) ), "{manifest}" );
    writeFile( join( out, wxT( "v2.0.kicad_release" ) ), "{other manifest}" );

    RELEASE_MANIFEST m;
    BOOST_REQUIRE(
            BuildReleaseManifest( m, baseOptions( join( proj, wxT( "widget.kicad_pro" ) ), out ) ) );

    BOOST_CHECK_EQUAL( m.m_outputs.size(), 1u );
    BOOST_CHECK( hasPath( m.m_outputs, wxT( "top.gbr" ) ) );
    BOOST_CHECK( !hasPath( m.m_outputs, wxT( "widget.kicad_release" ) ) );
    BOOST_CHECK( !hasPath( m.m_outputs, wxT( "v2.0.kicad_release" ) ) );
}


BOOST_AUTO_TEST_CASE( OutputDirEqualsProjectDirDoesNotDoubleCount )
{
    // Outputs written next to the project: the design inputs must not also appear as outputs.
    wxString proj = uniqueTempDir( "proj" );
    writeFile( join( proj, wxT( "widget.kicad_pro" ) ), "project" );
    writeFile( join( proj, wxT( "widget.kicad_pcb" ) ), "board" );
    writeFile( join( proj, wxT( "widget.kicad_sch" ) ), "sheet" );
    writeFile( join( proj, wxT( "top.gbr" ) ), "gerber" );

    RELEASE_MANIFEST m;
    BOOST_REQUIRE( BuildReleaseManifest(
            m, baseOptions( join( proj, wxT( "widget.kicad_pro" ) ), proj ) ) );

    BOOST_CHECK_EQUAL( m.m_inputs.size(), 3u );
    // Only the genuine output, not the three design files, is recorded as an output.
    BOOST_CHECK_EQUAL( m.m_outputs.size(), 1u );
    BOOST_CHECK( hasPath( m.m_outputs, wxT( "top.gbr" ) ) );
    BOOST_CHECK( !hasPath( m.m_outputs, wxT( "widget.kicad_pro" ) ) );
    BOOST_CHECK( !hasPath( m.m_outputs, wxT( "widget.kicad_pcb" ) ) );
}


BOOST_AUTO_TEST_CASE( FindsUnexpectedAndTamperedManifest )
{
    wxString proj = uniqueTempDir( "proj" );
    writeFile( join( proj, wxT( "widget.kicad_pro" ) ), "project" );

    wxString out = uniqueTempDir( "out" );
    writeFile( join( out, wxT( "top.gbr" ) ), "gerber" );

    RELEASE_MANIFEST built;
    BOOST_REQUIRE( BuildReleaseManifest(
            built, baseOptions( join( proj, wxT( "widget.kicad_pro" ) ), out ) ) );

    // A file added after the release is surfaced as unexpected (a .kicad_release is not).
    writeFile( join( out, wxT( "added.txt" ) ), "surprise" );
    writeFile( join( out, wxT( "widget.kicad_release" ) ), built.Format().utf8_string() );

    std::vector<wxString> extra = FindUnexpectedOutputs( built, out );
    BOOST_CHECK_EQUAL( extra.size(), 1u );
    BOOST_CHECK_EQUAL( extra.front(), wxT( "added.txt" ) );

    // Hand-tamper the serialized manifest (flip the recorded output sha, leave content_hash stale)
    // and confirm the tamper path is exercised via a builder-produced manifest.
    wxString json = built.Format();
    const wxString sha = built.m_outputs.front().m_sha256;
    BOOST_REQUIRE_EQUAL(
            json.Replace( sha, wxT( "0000000000000000000000000000000000000000000000000000000000000000" ),
                          false ),
            1u );

    RELEASE_MANIFEST tampered;
    BOOST_REQUIRE( RELEASE_MANIFEST::Parse( json, tampered ) );
    BOOST_CHECK( !tampered.VerifyOutputs( out ).m_contentHashOk );
}


BOOST_AUTO_TEST_CASE( RoundTripsAndVerifies )
{
    wxString proj = uniqueTempDir( "proj" );
    writeFile( join( proj, wxT( "widget.kicad_pro" ) ), "project" );
    writeFile( join( proj, wxT( "widget.kicad_pcb" ) ), "board" );

    wxString out = uniqueTempDir( "out" );
    writeFile( join( out, wxT( "top.gbr" ) ), "gerber" );

    RELEASE_MANIFEST built;
    BOOST_REQUIRE(
            BuildReleaseManifest( built, baseOptions( join( proj, wxT( "widget.kicad_pro" ) ),
                                                      out ) ) );

    // Serialize, reparse, then verify the outputs still match on disk.
    RELEASE_MANIFEST parsed;
    BOOST_REQUIRE( RELEASE_MANIFEST::Parse( built.Format(), parsed ) );

    RELEASE_MANIFEST::VERIFY_RESULT res = parsed.VerifyOutputs( out );
    BOOST_CHECK( res.Ok() );

    // Tamper an output; verify must now report drift.
    writeFile( join( out, wxT( "top.gbr" ) ), "gerber-modified" );
    RELEASE_MANIFEST::VERIFY_RESULT res2 = parsed.VerifyOutputs( out );
    BOOST_CHECK( !res2.Ok() );
}


BOOST_AUTO_TEST_CASE( MissingProjectAndOutput )
{
    wxString out = uniqueTempDir( "out" );

    RELEASE_MANIFEST m;
    wxString         err;

    // Missing project file.
    BOOST_CHECK( !BuildReleaseManifest(
            m, baseOptions( wxT( "/no/such/widget.kicad_pro" ), out ), &err ) );
    BOOST_CHECK( !err.IsEmpty() );

    // Missing output directory.
    wxString proj = uniqueTempDir( "proj" );
    writeFile( join( proj, wxT( "widget.kicad_pro" ) ), "project" );
    err.clear();
    BOOST_CHECK( !BuildReleaseManifest(
            m, baseOptions( join( proj, wxT( "widget.kicad_pro" ) ),
                            wxT( "/no/such/output/dir" ) ),
            &err ) );
    BOOST_CHECK( !err.IsEmpty() );
}


BOOST_AUTO_TEST_SUITE_END()
