cmake_minimum_required(VERSION 3.13)

# 当 Demo 作为根项目时, 设置 EGE_DEMO_IS_ROOT_PROJECT 为 ON
if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
  set(EGE_DEMO_IS_ROOT_PROJECT ON)
  option(EGE_BUILD_DEMO_WITH_PREBUILT_LIBS "Build demo with prebuilt libraries" OFF)
endif()

if(EGE_DEMO_IS_ROOT_PROJECT AND EGE_BUILD_DEMO_WITH_PREBUILT_LIBS)
  # Toolchain selection must happen before CMake enters this project. In
  # particular, a macOS host uses its native AppleClang package by default;
  # Windows cross-compilation requires an explicit toolchain file.
  message(STATUS
    "Building demos with the prebuilt library for ${CMAKE_HOST_SYSTEM_NAME}")
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

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    if(CMAKE_HOST_UNIX)
      target_compile_definitions(${name} PRIVATE _FORTIFY_SOURCE=0)
      target_link_options(${name}
        PRIVATE -static)
    endif()
  endif()
endmacro()

# 增加可执行目标，作为 demos 的依赖
macro(ege_add_executable_demo name source)
  ege_add_executable(${name} ${source})

  set_property(TARGET ${name}
    PROPERTY EXCLUDE_FROM_ALL true)
  add_dependencies(demos ${name})

  # Visible demo launches are manual opt-in tests. The default CTest suite is
  # strictly headless and never starts an AppKit window.
  if(EGE_BUILD_TEST AND EGE_ENABLE_WINDOW_TESTS
      AND APPLE AND TARGET ege_native_demo_launch_smoke
      AND NOT "${name}" MATCHES "^camera_")
    add_test(NAME demo.${name}.launch
      COMMAND $<TARGET_FILE:ege_native_demo_launch_smoke> $<TARGET_FILE:${name}>)
    set_tests_properties(demo.${name}.launch PROPERTIES
      RUN_SERIAL TRUE
      SKIP_RETURN_CODE 77
      TIMEOUT 8)
  elseif(EGE_BUILD_TEST AND EGE_ENABLE_WINDOW_TESTS
      AND APPLE AND "${name}" MATCHES "^camera_")
    message(STATUS
      "Keeping ${name} build-only in CTest to avoid camera access and permission UI")
  endif()
endmacro()

foreach(DEMO_SRC ${DEMO_SRCS})
  get_filename_component(DEMO_NAME ${DEMO_SRC} NAME_WE)

  # Camera demos require the optional ccap submodule and platform capture
  # backend.  Keep ordinary drawing demos buildable in a shallow checkout.
  if(DEMO_NAME MATCHES "^camera_" AND NOT EGE_ENABLE_CAMERA_CAPTURE)
    message(STATUS "Skipping ${DEMO_NAME}: EGE camera capture is disabled")
    continue()
  endif()

  # A Windows .scr screensaver that directly uses preview-parent and
  # MessageBox APIs is not a portable EGE sample.
  if(DEMO_NAME STREQUAL "graph_star" AND NOT WIN32)
    message(STATUS "Skipping ${DEMO_NAME}: Windows screensaver demo")
    continue()
  endif()

  ege_add_executable_demo(${DEMO_NAME} ${DEMO_SRC})

  if(DEMO_NAME STREQUAL "game_cube_runner")
    # Cube Runner uses std::clamp throughout its software rasterizer.
    # Make that requirement explicit instead of relying on a compiler's
    # platform-specific default language mode.
    target_compile_features(${DEMO_NAME} PRIVATE cxx_std_17)
  elseif(DEMO_NAME MATCHES "^camera_")
    target_compile_features(${DEMO_NAME} PRIVATE cxx_std_17)
    target_compile_definitions(${DEMO_NAME} PRIVATE EGE_ENABLE_CAMERA_CAPTURE=1)
    if(APPLE)
      target_link_options(${DEMO_NAME} PRIVATE
        "-sectcreate" "__TEXT" "__info_plist"
        "${CMAKE_CURRENT_SOURCE_DIR}/macos-camera-info.plist")
    endif()
  endif()
endforeach()

if(WIN32)
  target_sources(graph_rotateimage PRIVATE egelogo.rc)
endif()

if(NOT CMAKE_HOST_UNIX)
  # GMP demos are excluded on Unix hosts due to compatibility issues or specific requirements.
  # If the exclusion is intended for specific Unix-like environments, refine the condition below.
  message(STATUS "Finding GMP Library")
  find_path(GMPXX_INCLUDE_DIR NAMES gmpxx.h)
  find_library(GMPXX_LIBRARY NAMES gmpxx)
  find_library(GMP_LIBRARY NAMES gmp)

  if(GMPXX_INCLUDE_DIR AND GMPXX_LIBRARY AND GMP_LIBRARY)
    message(STATUS "GMP Library found")
    file(
      GLOB GMP_DEMO_SRCS
      LIST_DIRECTORIES false
      CONFIGURE_DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/gmp-demo/*.cpp"
    )

    foreach(GMP_DEMO_SRC ${GMP_DEMO_SRCS})
      get_filename_component(DEMO_NAME ${GMP_DEMO_SRC} NAME_WE)
      ege_add_executable_demo(${DEMO_NAME} ${GMP_DEMO_SRC})
      target_include_directories(${DEMO_NAME} PRIVATE ${GMPXX_INCLUDE_DIR})
      target_link_libraries(${DEMO_NAME} ${GMPXX_LIBRARY} ${GMP_LIBRARY})
    endforeach()
  else()
    message(STATUS "GMP Library not found")
  endif()
endif()

if(EGE_DEMO_IS_ROOT_PROJECT AND EGE_BUILD_DEMO_WITH_PREBUILT_LIBS)
  file(GLOB COMPILE_TESTS_SRCS
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/compile-tests/*.cpp"
  )

  foreach(COMPILE_TEST_SRC ${COMPILE_TESTS_SRCS})
    get_filename_component(TEST_NAME ${COMPILE_TEST_SRC} NAME_WE)
    ege_add_executable_demo("test-compile-${TEST_NAME}" ${COMPILE_TEST_SRC})
  endforeach()
endif()
