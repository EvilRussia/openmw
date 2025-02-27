find_package(GTest 1.10 REQUIRED)
find_package(GMock 1.10 REQUIRED)

if (GTEST_FOUND AND GMOCK_FOUND)
    include_directories(SYSTEM ${GTEST_INCLUDE_DIRS})
    include_directories(SYSTEM ${GMOCK_INCLUDE_DIRS})

    file(GLOB UNITTEST_SRC_FILES
        ../openmw/mwworld/store.cpp
        ../openmw/mwworld/esmstore.cpp
        mwworld/test_store.cpp

        mwdialogue/test_keywordsearch.cpp

        mwscript/test_scripts.cpp

        esm/test_fixed_string.cpp
        esm/variant.cpp

        lua/test_lua.cpp
        lua/test_scriptscontainer.cpp
        lua/test_utilpackage.cpp
        lua/test_serialization.cpp
        lua/test_querypackage.cpp
        lua/test_configuration.cpp
        lua/test_i18n.cpp
        lua/test_storage.cpp

        lua/test_ui_content.cpp

        misc/test_stringops.cpp
        misc/test_endianness.cpp
        misc/test_resourcehelpers.cpp
        misc/progressreporter.cpp
        misc/compression.cpp

        nifloader/testbulletnifloader.cpp

        detournavigator/navigator.cpp
        detournavigator/settingsutils.cpp
        detournavigator/recastmeshbuilder.cpp
        detournavigator/gettilespositions.cpp
        detournavigator/recastmeshobject.cpp
        detournavigator/navmeshtilescache.cpp
        detournavigator/tilecachedrecastmeshmanager.cpp
        detournavigator/navmeshdb.cpp
        detournavigator/serialization.cpp
        detournavigator/asyncnavmeshupdater.cpp

        serialization/binaryreader.cpp
        serialization/binarywriter.cpp
        serialization/sizeaccumulator.cpp
        serialization/integration.cpp

        settings/parser.cpp

        shader/parsedefines.cpp
        shader/parsefors.cpp
        shader/shadermanager.cpp

        ../openmw/options.cpp
        openmw/options.cpp

        sqlite3/db.cpp
        sqlite3/request.cpp
        sqlite3/statement.cpp
        sqlite3/transaction.cpp

        esmloader/load.cpp
        esmloader/esmdata.cpp

        files/hash.cpp
    )

    source_group(apps\\openmw_test_suite FILES openmw_test_suite.cpp ${UNITTEST_SRC_FILES})

    openmw_add_executable(openmw_test_suite openmw_test_suite.cpp ${UNITTEST_SRC_FILES})

    target_link_libraries(openmw_test_suite ${GMOCK_LIBRARIES} components)
    # Fix for not visible pthreads functions for linker with glibc 2.15
    if (UNIX AND NOT APPLE)
        target_link_libraries(openmw_test_suite ${CMAKE_THREAD_LIBS_INIT})
    endif()

    if (BUILD_WITH_CODE_COVERAGE)
        add_definitions(--coverage)
        target_link_libraries(openmw_test_suite gcov)
    endif()

    file(DOWNLOAD
        https://gitlab.com/OpenMW/example-suite/-/raw/8966dab24692555eec720c854fb0f73d108070cd/data/template.omwgame
        ${CMAKE_CURRENT_BINARY_DIR}/data/template.omwgame
        EXPECTED_MD5 bf3691034a38611534c74c3b89a7d2c3
    )

    target_compile_definitions(openmw_test_suite PRIVATE OPENMW_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data")

endif()
