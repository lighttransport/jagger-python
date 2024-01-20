cmake_minimum_required(VERSION 3.16)

set(EXE_TARGET "jagger")
set(PY_TARGET "jagger_ext")
project(${EXE_TARGET} CXX)

option(JAGGER_WITH_PYTHON "Build Python module(For developer)." On)
option(
    JAGGER_PREFER_LOCAL_PYTHON_INSTALLATION
    "Prefer locally-installed Python interpreter than system or conda/brew installed Python. Please specify   your Python interpreter with `Python3_EXECUTABLE` cmake option if you enable this option."
    OFF)


# cmake modules
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/sanitizers)
find_package(Sanitizers) # Address sanitizer (-DSANITIZE_ADDRESS=ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build standalone .so(for developer)
if (JAGGER_WITH_PYTHON)

  if(JAGGER_PREFER_LOCAL_PYTHON_INSTALLATION)
    #message(STATUS "Local Python")
    set(Python3_FIND_FRAMEWORK NEVER) # Do not search framework python
    set(Python3_FIND_STRATEGY LOCATION)
    set(Python3_FIND_REGISTRY NEVER) # Windows only
  else()
    set(Python3_FIND_FRAMEWORK LAST
    )# Prefer Brew/Conda to Apple framework python
  endif()

  find_package(
    Python3
    COMPONENTS Interpreter Development
    REQUIRED)

  find_package(pybind11 CONFIG)

  # pybind11 method:
  pybind11_add_module(${PY_TARGET} jagger/python-binding-jagger.cc)

  # copy .so to jagger/ after the build.
  add_custom_command(
    TARGET ${PY_TARGET}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${PY_TARGET}>"
            "${CMAKE_SOURCE_DIR}/jagger/$<TARGET_FILE_NAME:${PY_TARGET}>"
    COMMENT "copying jagger python module file to jagger/"
    VERBATIM)

endif()


add_executable(${EXE_TARGET} cpp_cli/jagger-app.cc)
add_sanitizers(${EXE_TARGET})

target_include_directories(${EXE_TARGET} PRIVATE jagger)

# enable mmap by default.
target_compile_definitions(${EXE_TARGET} PRIVATE "JAGGER_USE_MMAP_IO")

# [VisualStudio]
if(WIN32)
  # Set ${EXE_TARGET} as a startup project for VS IDE
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${EXE_TARGET})

  # For easier debugging in VS IDE(cmake 3.8.0 or later required) Set working
  # directory where CMakeLists.txt is placed.
  if(CMAKE_VERSION VERSION_GREATER 3.8.0)
    set_target_properties(
      ${EXE_TARGET} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY
                                           "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
endif()
