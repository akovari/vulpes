include_guard(GLOBAL)

if(DEFINED VCPKG_TARGET_TRIPLET AND VCPKG_TARGET_TRIPLET MATCHES "^([^-]+)-")
    set(VULPES_PACKAGE_ARCH "${CMAKE_MATCH_1}")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(VULPES_PACKAGE_ARCH "x64")
else()
    set(VULPES_PACKAGE_ARCH "x86")
endif()

if(WIN32)
    set(VULPES_PACKAGE_OS "windows")
elseif(APPLE)
    set(VULPES_PACKAGE_OS "macos")
else()
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" VULPES_PACKAGE_OS)
endif()
set(VULPES_PACKAGE_PLATFORM "${VULPES_PACKAGE_OS}-${VULPES_PACKAGE_ARCH}")

set(CPACK_PACKAGE_NAME "Vulpes")
set(CPACK_PACKAGE_VENDOR "Vulpes contributors")
set(CPACK_PACKAGE_CONTACT "adam@kovari.eu")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Local-first, keyboard-driven SQLite RAD environment")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/akovari/vulpes")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_FILE_NAME "vulpes-${VULPES_BUILD_VERSION}-${VULPES_PACKAGE_PLATFORM}")
set(CPACK_PACKAGE_CHECKSUM "SHA256")
set(CPACK_VERBATIM_VARIABLES YES)

# ZIP works consistently on every supported platform. Native installers are an
# explicit later release concern because they need platform signing/notarization.
set(CPACK_GENERATOR "ZIP")

include(CPack)
