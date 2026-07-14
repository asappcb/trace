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

#include <release/release_manifest.h>

#include <wx/ffile.h>
#include <wx/filename.h>
#include <wx/time.h>

#include <atomic>
#include <string>


namespace
{

// SHA-256 test vectors (NIST / well known).
const wxString SHA_ABC =
        wxT( "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" );
const wxString SHA_EMPTY =
        wxT( "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" );


wxString uniqueTempDir()
{
    static std::atomic<unsigned> counter{ 0 };

    wxString leaf = wxString::Format( wxT( "kicad-release-%ld-%u" ),
                                      static_cast<long>( wxGetLocalTimeMillis().GetValue() ),
                                      counter.fetch_add( 1 ) );
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

} // namespace


BOOST_AUTO_TEST_SUITE( ReleaseManifest )


BOOST_AUTO_TEST_CASE( HashFileKnownVectors )
{
    wxString dir = uniqueTempDir();

    wxString abc = join( dir, wxT( "abc.bin" ) );
    writeFile( abc, "abc" );

    bool     ok = false;
    uint64_t bytes = 0;
    wxString hash = ReleaseHashFile( abc, &ok, &bytes );

    BOOST_CHECK( ok );
    BOOST_CHECK_EQUAL( bytes, 3u );
    BOOST_CHECK_EQUAL( hash, SHA_ABC );

    wxString empty = join( dir, wxT( "empty.bin" ) );
    writeFile( empty, "" );

    hash = ReleaseHashFile( empty, &ok, &bytes );
    BOOST_CHECK( ok );
    BOOST_CHECK_EQUAL( bytes, 0u );
    BOOST_CHECK_EQUAL( hash, SHA_EMPTY );
}


BOOST_AUTO_TEST_CASE( HashFileMissing )
{
    bool     ok = true;
    uint64_t bytes = 123;
    wxString hash = ReleaseHashFile( wxT( "/nonexistent/path/does/not/exist.bin" ), &ok, &bytes );

    BOOST_CHECK( !ok );
    BOOST_CHECK_EQUAL( bytes, 0u );
    BOOST_CHECK( hash.IsEmpty() );
}


BOOST_AUTO_TEST_CASE( JsonRoundTrip )
{
    RELEASE_MANIFEST m;
    m.m_releaseName = wxT( "v1.2.0" );
    m.m_createdUtc = wxT( "2026-07-13T12:00:00Z" );
    m.m_projectName = wxT( "widget" );
    m.m_jobsetName = wxT( "production" );
    m.m_kicadVersion = wxT( "10.99.0" );
    m.m_vcsHash = wxT( "0123456789abcdef0123456789abcdef01234567" );
    m.m_vcsBranch = wxT( "master" );
    m.m_vcsDirty = true;
    m.AddInput( wxT( "widget.kicad_pcb" ), SHA_ABC, 3 );
    m.AddInput( wxT( "widget.kicad_pro" ), SHA_EMPTY, 0 );
    m.AddOutput( wxT( "gerbers/top.gbr" ), SHA_ABC, 3 );

    wxString json = m.Format();

    RELEASE_MANIFEST parsed;
    wxString         err;
    BOOST_REQUIRE_MESSAGE( RELEASE_MANIFEST::Parse( json, parsed, &err ), err );

    BOOST_CHECK_EQUAL( parsed.m_releaseName, m.m_releaseName );
    BOOST_CHECK_EQUAL( parsed.m_createdUtc, m.m_createdUtc );
    BOOST_CHECK_EQUAL( parsed.m_projectName, m.m_projectName );
    BOOST_CHECK_EQUAL( parsed.m_jobsetName, m.m_jobsetName );
    BOOST_CHECK_EQUAL( parsed.m_kicadVersion, m.m_kicadVersion );
    BOOST_CHECK_EQUAL( parsed.m_vcsHash, m.m_vcsHash );
    BOOST_CHECK_EQUAL( parsed.m_vcsBranch, m.m_vcsBranch );
    BOOST_CHECK( parsed.m_vcsDirty );

    BOOST_REQUIRE_EQUAL( parsed.m_inputs.size(), 2u );
    BOOST_REQUIRE_EQUAL( parsed.m_outputs.size(), 1u );
    BOOST_CHECK_EQUAL( parsed.m_inputs[0].m_path, wxT( "widget.kicad_pcb" ) );
    BOOST_CHECK_EQUAL( parsed.m_inputs[0].m_sha256, SHA_ABC );
    BOOST_CHECK_EQUAL( parsed.m_inputs[0].m_bytes, 3u );
    BOOST_CHECK_EQUAL( parsed.m_outputs[0].m_path, wxT( "gerbers/top.gbr" ) );

    // The declared content hash in the serialized form matches a fresh recomputation.
    BOOST_CHECK_EQUAL( parsed.DeclaredContentHash(), m.ComputeContentHash() );
    BOOST_CHECK_EQUAL( parsed.ComputeContentHash(), m.ComputeContentHash() );
}


BOOST_AUTO_TEST_CASE( ContentHashOrderIndependent )
{
    RELEASE_MANIFEST a;
    a.AddInput( wxT( "a.txt" ), SHA_ABC, 3 );
    a.AddInput( wxT( "b.txt" ), SHA_EMPTY, 0 );
    a.AddOutput( wxT( "out/x.gbr" ), SHA_ABC, 3 );

    RELEASE_MANIFEST b;
    b.AddOutput( wxT( "out/x.gbr" ), SHA_ABC, 3 );
    b.AddInput( wxT( "b.txt" ), SHA_EMPTY, 0 );
    b.AddInput( wxT( "a.txt" ), SHA_ABC, 3 );

    BOOST_CHECK_EQUAL( a.ComputeContentHash(), b.ComputeContentHash() );
    BOOST_CHECK_EQUAL( a.ComputeContentHash().length(), 64u );
}


BOOST_AUTO_TEST_CASE( ContentHashSensitiveToChanges )
{
    RELEASE_MANIFEST base;
    base.AddOutput( wxT( "x.gbr" ), SHA_ABC, 3 );
    wxString baseHash = base.ComputeContentHash();

    // Different SHA -> different content hash.
    RELEASE_MANIFEST diffSha;
    diffSha.AddOutput( wxT( "x.gbr" ), SHA_EMPTY, 0 );
    BOOST_CHECK_NE( diffSha.ComputeContentHash(), baseHash );

    // Same SHA but recorded as an input rather than an output -> different content hash
    // (the kind tag participates).
    RELEASE_MANIFEST asInput;
    asInput.AddInput( wxT( "x.gbr" ), SHA_ABC, 3 );
    BOOST_CHECK_NE( asInput.ComputeContentHash(), baseHash );
}


BOOST_AUTO_TEST_CASE( VerifyAllOk )
{
    wxString dir = uniqueTempDir();
    writeFile( join( dir, wxT( "top.gbr" ) ), "abc" );
    writeFile( join( dir, wxT( "drill/plated.drl" ) ), "abc" );

    RELEASE_MANIFEST m;
    m.AddOutput( wxT( "top.gbr" ), SHA_ABC, 3 );
    m.AddOutput( wxT( "drill/plated.drl" ), SHA_ABC, 3 );

    // Round-trip through JSON so the declared content hash is populated (real-world path).
    RELEASE_MANIFEST parsed;
    BOOST_REQUIRE( RELEASE_MANIFEST::Parse( m.Format(), parsed ) );

    RELEASE_MANIFEST::VERIFY_RESULT res = parsed.VerifyOutputs( dir );

    BOOST_CHECK( res.m_contentHashOk );
    BOOST_REQUIRE_EQUAL( res.m_entries.size(), 2u );

    for( const auto& e : res.m_entries )
        BOOST_CHECK_EQUAL( e.m_status, RELEASE_MANIFEST::VERIFY_ENTRY::OK );

    BOOST_CHECK( res.Ok() );
}


BOOST_AUTO_TEST_CASE( VerifyDetectsChangedAndMissing )
{
    wxString dir = uniqueTempDir();
    writeFile( join( dir, wxT( "changed.gbr" ) ), "abc" );
    // "missing.gbr" intentionally not written.

    RELEASE_MANIFEST m;
    m.AddOutput( wxT( "changed.gbr" ), SHA_ABC, 3 );
    m.AddOutput( wxT( "missing.gbr" ), SHA_ABC, 3 );

    // Now tamper with the on-disk file so its hash no longer matches.
    writeFile( join( dir, wxT( "changed.gbr" ) ), "abcd" );

    RELEASE_MANIFEST::VERIFY_RESULT res = m.VerifyOutputs( dir );

    BOOST_REQUIRE_EQUAL( res.m_entries.size(), 2u );
    BOOST_CHECK_EQUAL( res.m_entries[0].m_status, RELEASE_MANIFEST::VERIFY_ENTRY::CHANGED );
    BOOST_CHECK_EQUAL( res.m_entries[1].m_status, RELEASE_MANIFEST::VERIFY_ENTRY::MISSING );
    BOOST_CHECK( !res.Ok() );
}


BOOST_AUTO_TEST_CASE( VerifyDetectsManifestTampering )
{
    wxString dir = uniqueTempDir();
    writeFile( join( dir, wxT( "top.gbr" ) ), "abc" );

    RELEASE_MANIFEST m;
    m.AddOutput( wxT( "top.gbr" ), SHA_ABC, 3 );

    wxString json = m.Format();

    // Sanity: an untouched manifest verifies clean.
    RELEASE_MANIFEST parsed;
    BOOST_REQUIRE( RELEASE_MANIFEST::Parse( json, parsed ) );
    BOOST_CHECK( parsed.VerifyOutputs( dir ).m_contentHashOk );

    // Hand-tamper: flip the single output entry's recorded sha while leaving the top-level
    // content_hash (a different hex value) untouched -- i.e. someone edited the manifest by hand
    // without recomputing its fingerprint. SHA_ABC occurs exactly once (the entry), so a
    // first-occurrence replace cannot touch content_hash.
    wxString tampered = json;
    BOOST_REQUIRE_EQUAL( tampered.Replace( SHA_ABC, SHA_EMPTY, /* replaceAll */ false ), 1u );

    RELEASE_MANIFEST tamperedParsed;
    BOOST_REQUIRE( RELEASE_MANIFEST::Parse( tampered, tamperedParsed ) );

    RELEASE_MANIFEST::VERIFY_RESULT res = tamperedParsed.VerifyOutputs( dir );
    BOOST_CHECK( !res.m_contentHashOk );
    BOOST_CHECK( !res.Ok() );
}


BOOST_AUTO_TEST_CASE( ParseRejectsGarbage )
{
    RELEASE_MANIFEST out;
    wxString         err;

    BOOST_CHECK( !RELEASE_MANIFEST::Parse( wxT( "this is not json" ), out, &err ) );
    BOOST_CHECK( !err.IsEmpty() );

    BOOST_CHECK( !RELEASE_MANIFEST::Parse( wxT( "[1,2,3]" ), out ) ); // JSON, but not an object
}


BOOST_AUTO_TEST_CASE( ParseRejectsWrongTypedFields )
{
    // Each is a valid JSON object (passes the top-level gate) but has one field of the wrong
    // type. Parse must report failure gracefully -- never let an nlohmann type_error escape.
    const wxString bad[] = {
        wxT( R"({"schema_version": "not-a-number"})" ),
        wxT( R"({"release_name": 5})" ),
        wxT( R"({"vcs": {"dirty": "yes"}})" ),
        wxT( R"({"inputs": "should-be-an-array"})" ),
        wxT( R"({"outputs": [{"path": 5, "sha256": "x"}]})" ),
        wxT( R"({"outputs": [{"path": "p", "sha256": "s", "bytes": "three"}]})" ),
    };

    for( const wxString& json : bad )
    {
        RELEASE_MANIFEST out;
        wxString         err;
        BOOST_CHECK_MESSAGE( !RELEASE_MANIFEST::Parse( json, out, &err ),
                             "expected Parse to reject: " + json );
    }
}


BOOST_AUTO_TEST_CASE( WriteThenLoadFile )
{
    wxString dir = uniqueTempDir();
    wxString path = join( dir, wxT( "widget.kicad_release" ) );

    RELEASE_MANIFEST m;
    m.m_releaseName = wxT( "rc1" );
    m.AddOutput( wxT( "a.gbr" ), SHA_ABC, 3 );

    wxString err;
    BOOST_REQUIRE_MESSAGE( m.WriteToFile( path, &err ), err );

    RELEASE_MANIFEST loaded;
    BOOST_REQUIRE_MESSAGE( RELEASE_MANIFEST::LoadFromFile( path, loaded, &err ), err );

    BOOST_CHECK_EQUAL( loaded.m_releaseName, wxT( "rc1" ) );
    BOOST_REQUIRE_EQUAL( loaded.m_outputs.size(), 1u );
    BOOST_CHECK_EQUAL( loaded.DeclaredContentHash(), m.ComputeContentHash() );
}


BOOST_AUTO_TEST_SUITE_END()
