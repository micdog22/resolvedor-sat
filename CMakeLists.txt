cmake_minimum_required(VERSION 3.16)
project(resolvedor_sat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(sat_core STATIC src/solver.cpp src/cnf.cpp src/encoders.cpp)
target_include_directories(sat_core PUBLIC src)

add_executable(sat src/main.cpp)
add_executable(testes tests/testes.cpp)

foreach(alvo sat_core sat testes)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${alvo} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()
target_link_libraries(sat PRIVATE sat_core)
target_link_libraries(testes PRIVATE sat_core)

enable_testing()
add_test(NAME unidade COMMAND testes ${CMAKE_CURRENT_SOURCE_DIR}/exemplos)
add_test(NAME linha_de_comando COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli.sh $<TARGET_FILE:sat> ${CMAKE_CURRENT_SOURCE_DIR}/exemplos)
