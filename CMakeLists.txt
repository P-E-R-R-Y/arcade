#----------- general -----------

cmake_minimum_required(VERSION 3.24)
project(arcade)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#----------- project -----------

list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(CMakeUtils REQUIRED)
include(${cmakeutils_SOURCE_DIR}/ToolVar.cmake)

# La borne ne linke AUCUN vendor : ni sfml, ni raylib, ni aucun jeu. Elle
# n'a que des contrats et de quoi ouvrir une dll. C'est la seule maniere de
# pouvoir en ajouter un sans la recompiler.
find_package(System REQUIRED)
find_package(icore REQUIRED)
find_package(imodule REQUIRED)
find_package(igraphic REQUIRED)

add_executable(${PROJECT_NAME} sources/main.cpp)

#----------- dependencies -----------

target_link_libraries(${PROJECT_NAME} PRIVATE
  icore
  igraphic
  imodule
  ${CMAKE_DL_LIBS}
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/includes
)

# Le dossier balaye au demarrage, surchargeable par argv[1].
target_compile_definitions(${PROJECT_NAME} PRIVATE
  LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/lib"
  ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

#----------- tests -----------

find_package(GoogleTest REQUIRED)

enable_testing()

file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)

if (TEST_SOURCES)
  add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})

  target_include_directories(${PROJECT_NAME}_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
  )
  target_compile_definitions(${PROJECT_NAME}_tests PRIVATE
    LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/lib"
  ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
  )
  target_link_libraries(${PROJECT_NAME}_tests
    gtest_main
    icore
    igraphic
    imodule
    ${CMAKE_DL_LIBS}
  )

  include(GoogleTest)
  gtest_discover_tests(${PROJECT_NAME}_tests
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
  )
endif()
