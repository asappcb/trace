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

#include <release/release_manifest.h>

#include <algorithm>
#include <string>
#include <vector>

#include <wx/ffile.h>
#include <wx/filename.h>

#include <nlohmann/json.hpp>
#include <picosha2.h>


static std::string toU8( const wxString& aStr )
{
    return std::string( aStr.utf8_str() );
}


void RELEASE_MANIFEST::AddInput( const wxString& aRelPath, const wxString& aSha256, uint64_t aBytes )
{
    m_inputs.push_back( { aRelPath, aSha256, aBytes } );
}


void RELEASE_MANIFEST::AddOutput( const wxString& aRelPath, const wxString& aSha256,
                                  uint64_t aBytes )
{
    m_outputs.push_back( { aRelPath, aSha256, aBytes } );
}


wxString RELEASE_MANIFEST::ComputeContentHash() const
{
    // Build one canonical line per entry, then sort the lines so the digest is independent of
    // the order entries were added. The kind tag ('I'/'O') keeps an input from cancelling an
    // identical-path output.
    //
    // Each row is a JSON array dumped to a string: nlohmann escapes every control character
    // (including the '\n' used as the row separator) inside string values, so the encoding is
    // injective -- a path cannot contain bytes that forge the row/field structure of a different
    // entry set.
    std::vector<std::string> lines;
    lines.reserve( m_inputs.size() + m_outputs.size() );

    auto emit = [&]( char aKind, const RELEASE_FILE_ENTRY& e )
    {
        nlohmann::json row = nlohmann::json::array(
                { std::string( 1, aKind ), toU8( e.m_path ), toU8( e.m_sha256 ), e.m_bytes } );
        lines.push_back( row.dump() );
    };

    for( const RELEASE_FILE_ENTRY& e : m_inputs )
        emit( 'I', e );

    for( const RELEASE_FILE_ENTRY& e : m_outputs )
        emit( 'O', e );

    std::sort( lines.begin(), lines.end() );

    std::string canonical;

    for( const std::string& line : lines )
    {
        canonical += line;
        canonical += '\n';
    }

    return wxString::FromUTF8( picosha2::hash256_hex_string( canonical ).c_str() );
}


static nlohmann::json entryToJson( const RELEASE_FILE_ENTRY& e )
{
    return nlohmann::json{ { "path", toU8( e.m_path ) },
                           { "sha256", toU8( e.m_sha256 ) },
                           { "bytes", e.m_bytes } };
}


wxString RELEASE_MANIFEST::Format() const
{
    nlohmann::json j;

    j["schema_version"] = m_schemaVersion;
    j["release_name"] = toU8( m_releaseName );
    j["created_utc"] = toU8( m_createdUtc );
    j["project_name"] = toU8( m_projectName );
    j["jobset_name"] = toU8( m_jobsetName );
    j["kicad_version"] = toU8( m_kicadVersion );

    j["vcs"] = nlohmann::json{ { "hash", toU8( m_vcsHash ) },
                               { "branch", toU8( m_vcsBranch ) },
                               { "dirty", m_vcsDirty } };

    nlohmann::json inputs = nlohmann::json::array();

    for( const RELEASE_FILE_ENTRY& e : m_inputs )
        inputs.push_back( entryToJson( e ) );

    nlohmann::json outputs = nlohmann::json::array();

    for( const RELEASE_FILE_ENTRY& e : m_outputs )
        outputs.push_back( entryToJson( e ) );

    j["inputs"] = std::move( inputs );
    j["outputs"] = std::move( outputs );

    // Emitted last so a reader can recompute and compare.
    j["content_hash"] = toU8( ComputeContentHash() );

    return wxString::FromUTF8( j.dump( 2 ).c_str() );
}


bool RELEASE_MANIFEST::WriteToFile( const wxString& aPath, wxString* aError ) const
{
    wxFFile file( aPath, wxT( "wb" ) );

    if( !file.IsOpened() )
    {
        if( aError )
            *aError = wxString::Format( wxT( "Cannot open '%s' for writing" ), aPath );

        return false;
    }

    if( !file.Write( Format(), wxConvUTF8 ) )
    {
        if( aError )
            *aError = wxString::Format( wxT( "Failed writing to '%s'" ), aPath );

        return false;
    }

    return true;
}


static bool parseEntry( const nlohmann::json& j, RELEASE_FILE_ENTRY& aOut )
{
    if( !j.is_object() || !j.contains( "path" ) || !j.contains( "sha256" ) )
        return false;

    aOut.m_path = wxString::FromUTF8( j.at( "path" ).get<std::string>().c_str() );
    aOut.m_sha256 = wxString::FromUTF8( j.at( "sha256" ).get<std::string>().c_str() );
    aOut.m_bytes = j.value( "bytes", static_cast<uint64_t>( 0 ) );
    return true;
}


bool RELEASE_MANIFEST::Parse( const wxString& aJson, RELEASE_MANIFEST& aOut, wxString* aError )
{
    nlohmann::json j = nlohmann::json::parse( toU8( aJson ), nullptr, false );

    if( j.is_discarded() || !j.is_object() )
    {
        if( aError )
            *aError = wxT( "Release manifest is not valid JSON" );

        return false;
    }

    RELEASE_MANIFEST out;

    // Every field extraction below can throw nlohmann::json::type_error when a key is present
    // but holds the wrong JSON type (value()/get<>() only substitute the default when the key is
    // absent). Catch it so a corrupt or hand-edited manifest yields false + an error string
    // rather than an exception escaping this deliberately no-throw API.
    try
    {
        out.m_schemaVersion = j.value( "schema_version", 1 );
        out.m_releaseName = wxString::FromUTF8( j.value( "release_name", std::string() ).c_str() );
        out.m_createdUtc = wxString::FromUTF8( j.value( "created_utc", std::string() ).c_str() );
        out.m_projectName = wxString::FromUTF8( j.value( "project_name", std::string() ).c_str() );
        out.m_jobsetName = wxString::FromUTF8( j.value( "jobset_name", std::string() ).c_str() );
        out.m_kicadVersion =
                wxString::FromUTF8( j.value( "kicad_version", std::string() ).c_str() );

        if( j.contains( "vcs" ) && j["vcs"].is_object() )
        {
            const nlohmann::json& vcs = j["vcs"];
            out.m_vcsHash = wxString::FromUTF8( vcs.value( "hash", std::string() ).c_str() );
            out.m_vcsBranch = wxString::FromUTF8( vcs.value( "branch", std::string() ).c_str() );
            out.m_vcsDirty = vcs.value( "dirty", false );
        }

        auto readArray = [&]( const char* aKey, std::vector<RELEASE_FILE_ENTRY>& aDest ) -> bool
        {
            if( !j.contains( aKey ) )
                return true;

            if( !j[aKey].is_array() )
                return false;

            for( const nlohmann::json& e : j[aKey] )
            {
                RELEASE_FILE_ENTRY entry;

                if( !parseEntry( e, entry ) )
                    return false;

                aDest.push_back( std::move( entry ) );
            }

            return true;
        };

        if( !readArray( "inputs", out.m_inputs ) || !readArray( "outputs", out.m_outputs ) )
        {
            if( aError )
                *aError = wxT( "Release manifest has a malformed file entry" );

            return false;
        }

        out.m_declaredContentHash =
                wxString::FromUTF8( j.value( "content_hash", std::string() ).c_str() );
    }
    catch( const nlohmann::json::exception& exc )
    {
        if( aError )
            *aError = wxString::Format( wxT( "Release manifest has a malformed field: %s" ),
                                        wxString::FromUTF8( exc.what() ) );

        return false;
    }

    aOut = std::move( out );
    return true;
}


bool RELEASE_MANIFEST::LoadFromFile( const wxString& aPath, RELEASE_MANIFEST& aOut,
                                     wxString* aError )
{
    wxFFile file( aPath, wxT( "rb" ) );

    if( !file.IsOpened() )
    {
        if( aError )
            *aError = wxString::Format( wxT( "Cannot open '%s'" ), aPath );

        return false;
    }

    wxString contents;

    if( !file.ReadAll( &contents, wxConvUTF8 ) )
    {
        if( aError )
            *aError = wxString::Format( wxT( "Failed reading '%s'" ), aPath );

        return false;
    }

    return Parse( contents, aOut, aError );
}


bool RELEASE_MANIFEST::VERIFY_RESULT::Ok() const
{
    if( !m_contentHashOk )
        return false;

    for( const VERIFY_ENTRY& e : m_entries )
    {
        if( e.m_status != VERIFY_ENTRY::OK )
            return false;
    }

    return true;
}


RELEASE_MANIFEST::VERIFY_RESULT RELEASE_MANIFEST::VerifyOutputs( const wxString& aBaseDir ) const
{
    VERIFY_RESULT result;

    for( const RELEASE_FILE_ENTRY& e : m_outputs )
    {
        wxFileName fn( e.m_path );
        fn.MakeAbsolute( aBaseDir );

        VERIFY_ENTRY ve;
        ve.m_path = e.m_path;

        if( !fn.FileExists() )
        {
            ve.m_status = VERIFY_ENTRY::MISSING;
        }
        else
        {
            bool     ok = false;
            wxString actual = ReleaseHashFile( fn.GetFullPath(), &ok );

            if( !ok )
                ve.m_status = VERIFY_ENTRY::UNREADABLE; // present (FileExists) but unhashable
            else if( actual.IsSameAs( e.m_sha256, /* caseSensitive */ false ) )
                ve.m_status = VERIFY_ENTRY::OK;
            else
                ve.m_status = VERIFY_ENTRY::CHANGED;
        }

        result.m_entries.push_back( ve );
    }

    // A manifest whose entry list was edited without recomputing the declared content hash is
    // itself tampered with. Only meaningful when a hash was declared (parsed from a file).
    result.m_contentHashOk =
            m_declaredContentHash.IsEmpty() || m_declaredContentHash == ComputeContentHash();

    return result;
}


wxString ReleaseHashFile( const wxString& aPath, bool* aOk, uint64_t* aBytes )
{
    wxFFile file( aPath, wxT( "rb" ) );

    if( !file.IsOpened() )
    {
        if( aOk )
            *aOk = false;

        if( aBytes )
            *aBytes = 0;

        return wxEmptyString;
    }

    picosha2::hash256_one_by_one hasher;

    const size_t               CHUNK = 64 * 1024;
    std::vector<unsigned char> buf( CHUNK );
    uint64_t                   total = 0;

    for( ;; )
    {
        size_t n = file.Read( buf.data(), CHUNK );

        if( n > 0 )
        {
            hasher.process( buf.begin(), buf.begin() + n );
            total += n;
        }

        if( n < CHUNK )
            break; // short read == EOF (or error, reported below)
    }

    if( file.Error() )
    {
        if( aOk )
            *aOk = false;

        if( aBytes )
            *aBytes = 0;

        return wxEmptyString;
    }

    hasher.finish();

    if( aOk )
        *aOk = true;

    if( aBytes )
        *aBytes = total;

    return wxString::FromUTF8( picosha2::get_hash_hex_string( hasher ).c_str() );
}
