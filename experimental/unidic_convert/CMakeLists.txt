cmake_minimum_required(VERSION 3.16)

set(EXE_TARGET "dict_dump")
project(${EXE_TARGET} CXX)

# cmake modules
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/../../cmake)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/../../cmake/sanitizers)
find_package(Sanitizers) # Address sanitizer (-DSANITIZE_ADDRESS=ON)

set(MECAB_DIR "mecab/mecab/src")

set(SOURCE_FILES
  dict_dump.cc
  ${MECAB_DIR}/char_property.cpp
  ${MECAB_DIR}/dictionary.cpp
  ${MECAB_DIR}/dictionary_rewriter.cpp
  ${MECAB_DIR}/iconv_utils.cpp
  ${MECAB_DIR}/connector.cpp
  ${MECAB_DIR}/context_id.cpp
  ${MECAB_DIR}/feature_index.cpp
  ${MECAB_DIR}/string_buffer.cpp
  ${MECAB_DIR}/utils.cpp
  ${MECAB_DIR}/param.cpp
  ${MECAB_DIR}/writer.cpp
  ${MECAB_DIR}/tagger.cpp
  ${MECAB_DIR}/libmecab.cpp
  ${MECAB_DIR}/viterbi.cpp
  ${MECAB_DIR}/tokenizer.cpp
  ${MECAB_DIR}/nbest_generator.cpp
  )

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${EXE_TARGET} ${SOURCE_FILES})
add_sanitizers(${EXE_TARGET})

if (WIN32)
  # TODO
else()
  set(MECAB_COMPILE_DEFS ${MECAB_COMPILE_DEFS} "-DHAVE_STDINT_H -DHAVE_FCNTL_H -DHAVE_SYS_TYPES_H -DHAVE_SYS_STAT_H -DHAVE_STRING_H -DHAV_SYS_MMAN_H -DHAVE_UNISTD_H -DHAVE_DIRENT_H " )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # suppress warning of 'register' qualifier usage in mecab code.
  set(MECAB_COMPILE_FLAGS ${MECAB_COMPILE_FLAGS} "-Wno-deprecated-register ")
endif()

set_source_files_properties(${SOURCE_FILES} PROPERTIES COMPILE_FLAGS ${MECAB_COMPILE_FLAGS})
target_compile_definitions(${EXE_TARGET} PRIVATE ${MECAB_COMPILE_DEFS})
target_compile_definitions(${EXE_TARGET} PRIVATE PACKAGE="mecab")
target_compile_definitions(${EXE_TARGET} PRIVATE VERSION="0.996")
target_compile_definitions(${EXE_TARGET} PRIVATE DIC_VERSION=102)
target_compile_definitions(${EXE_TARGET} PRIVATE MECAB_DEFAULT_RC="mecabrc")
target_include_directories(${EXE_TARGET} PRIVATE ${MECAB_DIR})
#target_link_directories(${EXE_TARGET} PRIVATE ${MECAB_LIB_DIR})
#target_link_libraries(${EXE_TARGET} PRIVATE mecab)

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
