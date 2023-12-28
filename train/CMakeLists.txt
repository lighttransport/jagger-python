cmake_minimum_required(VERSION 3.16)

set(EXE_TARGET "train_jagger")
project(${EXE_TARGET} CXX)

# cmake modules
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/../cmake)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/../cmake/sanitizers)
find_package(Sanitizers) # Address sanitizer (-DSANITIZE_ADDRESS=ON)


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${EXE_TARGET} train_jagger.cc)

target_include_directories(${EXE_TARGET} PRIVATE ..)

target_compile_definitions(${EXE_TARGET} PRIVATE "JAGGER_DEFAULT_MODEL=\"/usr/local/lib/jagger/model/kwdlc\"")

#
target_compile_definitions(${EXE_TARGET} PRIVATE "NUM_POS_FIELD=4")
#target_compile_definitions(${EXE_TARGET} PRIVATE "USE_JUMANDIC=1")

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
