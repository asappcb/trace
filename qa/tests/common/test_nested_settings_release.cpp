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

/**
 * @file test_nested_settings_release.cpp
 * Destroying a JSON_SETTINGS that still owns nested settings must not walk freed storage.
 *
 * ~JSON_SETTINGS releases every entry of m_nested_settings, and ReleaseNestedSettings() erases
 * the entry it releases from that same vector.  Iterating the member directly therefore
 * invalidates the loop's own iterators on the first release, and the second iteration reads
 * freed storage -- a SIGSEGV that showed up in `kicad-cli pcb diff`, which unloads a project
 * whose PROJECT_FILE holds several nested settings.  Two nested entries are the minimum needed
 * to step past the invalidated end.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <settings/json_settings.h>
#include <settings/nested_settings.h>
#include <settings/settings_manager.h>

#include <memory>


namespace
{

class TEST_PARENT_SETTINGS : public JSON_SETTINGS
{
public:
    TEST_PARENT_SETTINGS() :
            JSON_SETTINGS( "test_parent", SETTINGS_LOC::NONE, 1 )
    {
    }

    bool LoadFromFile( const wxString& aDirectory = "" ) override { return true; }

    bool SaveToFile( const wxString& aDirectory = "", bool aForce = false ) override
    {
        return false;
    }
};


class TEST_NESTED_SETTINGS : public NESTED_SETTINGS
{
public:
    TEST_NESTED_SETTINGS( const std::string& aName, JSON_SETTINGS* aParent ) :
            NESTED_SETTINGS( aName, 1, aParent, aName, /* aLoadFromFile */ false )
    {
    }

    bool LoadFromFile( const wxString& aDirectory = "" ) override { return true; }

    bool SaveToFile( const wxString& aDirectory = "", bool aForce = false ) override
    {
        return false;
    }
};

} // namespace


BOOST_AUTO_TEST_SUITE( NestedSettingsRelease )


BOOST_AUTO_TEST_CASE( ParentDestructionReleasesEveryNestedSetting )
{
    SETTINGS_MANAGER manager;

    auto parent = std::make_unique<TEST_PARENT_SETTINGS>();

    // ReleaseNestedSettings() is a no-op without a manager, so the release path only runs -- and
    // the erase-during-iteration only bites -- once one is set.
    parent->SetManager( &manager );

    auto first = std::make_unique<TEST_NESTED_SETTINGS>( "nested_one", parent.get() );
    auto second = std::make_unique<TEST_NESTED_SETTINGS>( "nested_two", parent.get() );
    auto third = std::make_unique<TEST_NESTED_SETTINGS>( "nested_three", parent.get() );

    BOOST_REQUIRE_EQUAL( first->GetParent(), parent.get() );
    BOOST_REQUIRE_EQUAL( second->GetParent(), parent.get() );
    BOOST_REQUIRE_EQUAL( third->GetParent(), parent.get() );

    // Must not read freed storage.  Every child must also be unparented, not just the first:
    // a child left pointing at the destroyed parent dangles for the rest of its life.
    parent.reset();

    BOOST_CHECK_EQUAL( first->GetParent(), nullptr );
    BOOST_CHECK_EQUAL( second->GetParent(), nullptr );
    BOOST_CHECK_EQUAL( third->GetParent(), nullptr );
}


// De-registration used to be skipped entirely when the parent had no manager, while registration
// was unconditional.  A child destroyed during such a window stayed in the parent's list, and the
// parent's destructor then flushed freed memory.
BOOST_AUTO_TEST_CASE( ChildDestroyedWithoutManagerDoesNotDangle )
{
    auto parent = std::make_unique<TEST_PARENT_SETTINGS>();

    {
        TEST_NESTED_SETTINGS transient( "transient", parent.get() );
        BOOST_REQUIRE_EQUAL( transient.GetParent(), parent.get() );
    }

    // The child is gone.  Giving the parent a manager now means its destructor takes the flush
    // path, which is where a retained entry would be dereferenced.
    SETTINGS_MANAGER manager;
    parent->SetManager( &manager );

    parent.reset();
}


// SetParent() on an already-parented child must not register it a second time: the child
// de-registers exactly once when destroyed, so a duplicate entry would outlive it.
BOOST_AUTO_TEST_CASE( RepeatedSetParentDoesNotDuplicateRegistration )
{
    SETTINGS_MANAGER manager;

    auto parent = std::make_unique<TEST_PARENT_SETTINGS>();
    parent->SetManager( &manager );

    auto child = std::make_unique<TEST_NESTED_SETTINGS>( "child", parent.get() );
    child->SetParent( parent.get(), /* aLoadFromFile */ false );

    child.reset();

    parent.reset();
}


BOOST_AUTO_TEST_SUITE_END()
