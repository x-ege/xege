cmake_minimum_required(VERSION 3.13)

# 当 Demo 作为根项目时, 设置 EGE_DEMO_IS_ROOT_PROJECT 为 ON
if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
  set(EGE_DEMO_IS_ROOT_PROJECT ON)
  option(EGE_BUILD_DEMO_WITH_PREBUILT_LIBS "Build demo with prebuilt libraries" OFF)
endif()

if(EGE_DEMO_IS_ROOT_PROJECT AND EGE_BUILD_DEMO_WITH_PREBUILT_LIBS)
  # 以预编译好的 ege 的库作为依赖
  if(CMAKE_HOST_UNIX)
    set(CMAKE_SYSTEM_NAME Windows)
    set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

    # cross compilers to use for C, C++ and RC
    set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
    set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
    set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)

    # modify default behavior of FIND_XXX() commands
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(PKG_CONFIG_EXECUTABLE ${TOOLCHAIN_PREFIX}-pkg-config)
  endif()

  message(STATUS "Building demo with prebuilt libraries")
endif()

project(ege-demos)

if(EGE_DEMO_IS_ROOT_PROJECT AND EGE_BUILD_DEMO_WITH_PREBUILT_LIBS)
  include(./ege_release.cmake)
endif()

file(
  GLOB DEMO_SRCS
  LIST_DIRECTORIES false
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_custom_target(demos)

macro(ege_add_executable name source)
  message(STATUS "Adding executable: ${name} with source: ${source}")
  add_executable(${name} ${source})
  target_link_libraries(${name} xege)

  if(MSVC)
    target_compile_options(${name} PRIVATE
      /source-charset:utf-8
      /D_USE_MATH_DEFINES
      /D__STDC_LIMIT_MACROS
      /MP
    )
  endif()
endmacro()

# 增加可执行目标，作为 demos 的依赖
macro(ege_add_executable_demo name source)
  ege_add_executable(${name} ${source})

  set_property(TARGET ${name}
    PROPERTY EXCLUDE_FROM_ALL true)
  add_dependencies(demos ${DEMO_NAME})
endmacro()

foreach(DEMO_SRC ${DEMO_SRCS})
  get_filename_component(DEMO_NAME ${DEMO_SRC} NAME_WE)
  ege_add_executable_demo(${DEMO_NAME} ${DEMO_SRC})
endforeach()

target_sources(graph_rotateimage PRIVATE egelogo.rc)

if(NOT CMAKE_HOST_UNIX)
  # GMP demos are excluded on Unix hosts due to compatibility issues or specific requirements.
  # If the exclusion is intended for specific Unix-like environments, refine the condition below.
  message(CHECK_START "Finding GMP Library")
  find_library(gmp gmp)

  if(gmp)
    message(CHECK_PASS "found")
    file(
      GLOB GMP_DEMO_SRCS
      LIST_DIRECTORIES false
      CONFIGURE_DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/gmp-demo/*.cpp"
    )

    foreach(GMP_DEMO_SRC ${GMP_DEMO_SRCS})
      get_filename_component(DEMO_NAME ${GMP_DEMO_SRC} NAME_WE)
      ege_add_executable_demo(${DEMO_NAME} ${GMP_DEMO_SRC})
      target_link_libraries(${DEMO_NAME} gmp)
    endforeach()
  else()
    message(CHECK_PASS "not found")
  endif()
endif()
