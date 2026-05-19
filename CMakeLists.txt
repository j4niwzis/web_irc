cmake_minimum_required(VERSION 4.1.1 FATAL_ERROR)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
  # This specific value changes as experimental support evolves. See
  # `Help/dev/experimental.rst` in the CMake source corresponding to
  # your CMake build for the exact value to use.
  "d0edc3af-4c50-42ea-a356-e2862fe7a444")

set(CMAKE_CXX_MODULE_STD 1)

# ---- Project ----

# Note: update this to your new project's name and version
project(
  web_irc
  VERSION 0.1.2
  LANGUAGES CXX
)



# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

# ---- Add dependencies via CPM ----
# see https://github.com/TheLartians/CPM.cmake for more info

include(cmake/CPM.cmake)

find_package(Boost 1.88 REQUIRED)

CPMAddPackage("gh:j4niwzis/mirc@0.3.0")
CPMAddPackage("gh:avast/asio-mutex#main")

# ---- Add source files ----

# Note: globbing sources is considered bad practice as CMake's generators may not detect new files
# automatically. Keep that in mind when changing files, or explicitly mention them here.
file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")

# ---- Create library ----

# Note: for header-only libraries change all PUBLIC flags to INTERFACE and create an interface
# target: add_library(${PROJECT_NAME} INTERFACE)
add_executable(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/main.cc")

target_compile_definitions(${PROJECT_NAME} PRIVATE
  WEB_IRC_VERSION="${PROJECT_VERSION}"
  WEB_IRC_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  WEB_IRC_VERSION_MINOR=${PROJECT_VERSION_MINOR}
  WEB_IRC_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_26)


target_compile_options(${PROJECT_NAME} PRIVATE "-Wno-c23-extensions")


target_sources(${PROJECT_NAME} PUBLIC FILE_SET CXX_MODULES FILES ${sources})

# Link dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC
  Boost::headers
  mirc::mirc
  asio-mutex-header
)


# ---- Create an installable target ----
# this allows users to install and find the library via `find_package()`.

# the location where the project's version header will be placed should match the project's regular
# header paths

