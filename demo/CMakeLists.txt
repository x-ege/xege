cmake_minimum_required(VERSION 3.13)

project(ege-demos)

if(EGE_ENABLE_CPP17)
    # 将编译标准设置为 C++17, 仓库对外标准保持不变, 未引入额外的依赖.
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

file(
  GLOB DEMO_SRCS
  LIST_DIRECTORIES false
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  )

add_custom_target(demos)

# 增加可执行目标，作为 demos 的依赖
macro(ege_add_executable_demo name source)
  ege_add_executable(${name} ${source})
  
  set_property(TARGET ${name}
    PROPERTY EXCLUDE_FROM_ALL true)
  add_dependencies(demos ${DEMO_NAME})
endmacro()

foreach (DEMO_SRC ${DEMO_SRCS})
  get_filename_component(DEMO_NAME ${DEMO_SRC} NAME_WE)
  ege_add_executable_demo(${DEMO_NAME} ${DEMO_SRC})
endforeach ()
target_sources(graph_rotateimage PRIVATE egelogo.rc)

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
  foreach (GMP_DEMO_SRC ${GMP_DEMO_SRCS})
    get_filename_component(DEMO_NAME ${GMP_DEMO_SRC} NAME_WE)
    ege_add_executable_demo(${DEMO_NAME} ${GMP_DEMO_SRC})
    target_link_libraries(${DEMO_NAME} gmp)
  endforeach ()
else()
  message(CHECK_PASS "not found")
endif()
