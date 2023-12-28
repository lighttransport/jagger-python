cmake_minimum_required(VERSION 3.16)

set(EXE_TARGET "jagger")
project(${EXE_TARGET} CXX)

# cmake modules
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/sanitizers)
find_package(Sanitizers) # Address sanitizer (-DSANITIZE_ADDRESS=ON)

if(JAGGER_WITH_PYTHON)

  # TODO: Fully Remove pybind11-based python binding
  ## For the time beging, we stick with pybind11, since PyPI manylinux2014(probably mostly used architectrue as of 2022/Aug) does not support C++17.
  ## We may switch to nanobind at some point(probably around 2024?)

  ## build monolithic .dll
  ##nanobind_add_module(${BUILD_TARGET_PY} ${TINYUSDZ_PYTHON_BINDING_SOURCES})

  #pybind11_add_module(${BUILD_TARGET_PY} ${TINYUSDZ_PYTHON_BINDING_SOURCES})
  #add_sanitizers(${BUILD_TARGET_PY})
  #target_include_directories(
  #  ${BUILD_TARGET_PY} PRIVATE ${PROJECT_SOURCE_DIR}/src)

  #target_link_libraries(${BUILD_TARGET_PY} PRIVATE ${TINYUSDZ_TARGET_STATIC})

  ## Use 'c' prefix
  ## TODO: Use `cpp` prefix?
  #set_target_properties(${BUILD_TARGET_PY} PROPERTIES OUTPUT_NAME "ctinyusdz")

  ## copy python binding .so file to python/
  ## For developer
  ## NOTE: `POST_BUILD` command is not triggered when building python module using
  ## `python setup.py build`
  #add_custom_command(
  #  TARGET ${BUILD_TARGET_PY}
  #  POST_BUILD
  #  COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${BUILD_TARGET_PY}>"
  #          "${CMAKE_SOURCE_DIR}/python/tinyusdz/$<TARGET_FILE_NAME:${BUILD_TARGET_PY}>"
  #  COMMENT "copying python module file to python/tinyusdz"
  #  VERBATIM)
  ## For pypi packaging
  #install(TARGETS ${BUILD_TARGET_PY} LIBRARY DESTINATION tinyusdz)


  # copy C tinyusd .so file to python/tinyusdz/
  # For developer
  # NOTE: `POST_BUILD` command is not triggered when building python module using
  # `python setup.py build`
  add_custom_command(
    TARGET ${BUILD_TARGET_C}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${BUILD_TARGET_C}>"
            "${CMAKE_SOURCE_DIR}/python/tinyusdz/$<TARGET_FILE_NAME:${BUILD_TARGET_C}>"
    COMMENT "Copying libc-tinyusd.so/c-tinyusd.dll file to <tinyusdz>/python/tinyusdz"
    VERBATIM)

  # TODO: Run ctypesgen?

  #
  # target for setup.py
  # TODO: Use custom target and supply `cmake_install_target` in setup.py?
  # https://scikit-build.readthedocs.io/en/latest/usage.html#scikit-build-options
  #
  install(TARGETS ${BUILD_TARGET_C} LIBRARY DESTINATION tinyusdz)


endif()


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${EXE_TARGET} jagger-app.cc)

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
