cmake_minimum_required(VERSION 3.21)
project(GPUView VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
if(MSVC)
  add_compile_options(/utf-8 /W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets Test)
enable_testing()

add_library(gpuview_core
  src/core/trace_store.cpp src/core/render_query.cpp src/core/clock_mapping.cpp)
target_include_directories(gpuview_core PUBLIC src)
set_target_properties(gpuview_core PROPERTIES AUTOMOC OFF)

add_library(gpuview_adapters src/adapters/synthetic_source.cpp)
target_link_libraries(gpuview_adapters PUBLIC gpuview_core)
set_target_properties(gpuview_adapters PROPERTIES AUTOMOC OFF)

add_library(gpuview_application src/application/session_controller.cpp)
target_link_libraries(gpuview_application PUBLIC gpuview_adapters Qt6::Core)

add_library(gpuview_widgets src/ui_widgets/timeline_widget.cpp src/ui_widgets/main_window.cpp)
target_link_libraries(gpuview_widgets PUBLIC gpuview_application Qt6::Widgets)

qt_add_executable(GPUView WIN32 src/main.cpp)
target_link_libraries(GPUView PRIVATE gpuview_widgets)

qt_add_executable(gpuview_tests tests/core_tests.cpp)
set_target_properties(gpuview_tests PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(gpuview_tests PRIVATE gpuview_application Qt6::Test)
add_test(NAME core_and_lifecycle COMMAND gpuview_tests -o ${CMAKE_BINARY_DIR}/core-tests.txt,txt)

qt_add_executable(gpuview_ui_tests tests/ui_tests.cpp)
set_target_properties(gpuview_ui_tests PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(gpuview_ui_tests PRIVATE gpuview_widgets Qt6::Test)
add_test(NAME widget_interaction COMMAND gpuview_ui_tests -platform offscreen -o ${CMAKE_BINARY_DIR}/ui-tests.txt,txt)

add_executable(gpuview_benchmark tools/benchmark_main.cpp)
target_link_libraries(gpuview_benchmark PRIVATE gpuview_adapters)
if(WIN32)
  target_link_libraries(gpuview_benchmark PRIVATE psapi)
endif()
set_target_properties(gpuview_benchmark PROPERTIES AUTOMOC OFF)
install(TARGETS GPUView RUNTIME DESTINATION bin)
