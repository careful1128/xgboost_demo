cmake_minimum_required(VERSION 3.18)

find_package(Threads REQUIRED)

set(RABIT_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/src/allreduce_base.cc
  ${CMAKE_CURRENT_LIST_DIR}/src/rabit_c_api.cc)

if (RABIT_MOCK)
  list(APPEND RABIT_SOURCES ${CMAKE_CURRENT_LIST_DIR}/src/engine_mock.cc)
else ()
  list(APPEND RABIT_SOURCES ${CMAKE_CURRENT_LIST_DIR}/src/engine.cc)
endif ()

set(RABIT_SOURCES ${RABIT_SOURCES} PARENT_SCOPE)
