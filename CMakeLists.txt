# GPUView构建入口：按模块目录收集源文件，隔离应用/测试/基准各自的main。
# 公开VS工程只是相对路径的调试入口，实际编译目标和依赖关系由这里统一维护。
cmake_minimum_required(VERSION 3.21)
project(GPUView VERSION 0.3.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
if(MSVC)
  add_compile_options(/utf-8 /W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
# Widgets负责桌面UI，Core用于任务/原子导出适配，Test只用于回归目标。
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets Test)
enable_testing()
include(cmake/CollectSources.cmake)
gpuview_collect_sources(core_sources src/core)
gpuview_collect_sources(adapter_sources src/adapters)
gpuview_collect_sources(application_sources src/application)
gpuview_collect_sources(widget_sources src/ui_widgets)
gpuview_collect_sources(app_sources src/app)
gpuview_collect_sources(core_test_sources tests/core)
gpuview_collect_sources(ui_test_sources tests/ui)
gpuview_collect_sources(benchmark_sources tools/benchmark)

# 分层依赖自下而上；core关闭AUTOMOC并保持不依赖Qt。
add_library(gpuview_core ${core_sources})
target_include_directories(gpuview_core PUBLIC src)
set_target_properties(gpuview_core PROPERTIES AUTOMOC OFF)

add_library(gpuview_adapters ${adapter_sources})
target_link_libraries(gpuview_adapters PUBLIC gpuview_core Qt6::Core)
set_target_properties(gpuview_adapters PROPERTIES AUTOMOC OFF)

add_library(gpuview_application ${application_sources})
target_link_libraries(gpuview_application PUBLIC gpuview_adapters Qt6::Core)

add_library(gpuview_widgets ${widget_sources})
target_link_libraries(gpuview_widgets PUBLIC gpuview_application Qt6::Widgets)

qt_add_executable(GPUView WIN32 ${app_sources})
target_link_libraries(GPUView PRIVATE gpuview_widgets)

# 测试保留控制台输出，日志按配置区分，避免Debug/Release互相覆盖证据。
qt_add_executable(gpuview_tests ${core_test_sources})
set_target_properties(gpuview_tests PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(gpuview_tests PRIVATE gpuview_application Qt6::Test)
add_test(NAME core_and_lifecycle COMMAND gpuview_tests -o ${CMAKE_BINARY_DIR}/core-tests-$<CONFIG>.txt,txt)

qt_add_executable(gpuview_ui_tests ${ui_test_sources})
set_target_properties(gpuview_ui_tests PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(gpuview_ui_tests PRIVATE gpuview_widgets Qt6::Test)
add_test(NAME widget_interaction COMMAND gpuview_ui_tests -platform offscreen -o ${CMAKE_BINARY_DIR}/ui-tests-$<CONFIG>.txt,txt)
if(WIN32)
  # windeployqt部署的是桌面插件；测试单独从SDK加载离屏插件，不污染应用调试配置。
  set_tests_properties(widget_interaction PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:Qt6::Core>/../plugins/platforms")
endif()

# 性能工具是独立程序，不把测量代码放进用户应用路径；psapi只用于Windows内存计数。
add_executable(gpuview_benchmark ${benchmark_sources})
target_link_libraries(gpuview_benchmark PRIVATE gpuview_adapters)
if(WIN32)
  target_link_libraries(gpuview_benchmark PRIVATE psapi)
endif()
set_target_properties(gpuview_benchmark PROPERTIES AUTOMOC OFF)
install(TARGETS GPUView RUNTIME DESTINATION bin)

# 独立入口按目录收集，不与应用main混编。
gpuview_collect_sources(ui_benchmark_sources tools/ui_benchmark)
qt_add_executable(gpuview_ui_benchmark ${ui_benchmark_sources})
target_link_libraries(gpuview_ui_benchmark PRIVATE gpuview_widgets)
if(WIN32)
  target_link_libraries(gpuview_ui_benchmark PRIVATE psapi)
  gpuview_collect_sources(capture_target_sources tools/capture_target)
  add_executable(gpuview_capture_target WIN32 ${capture_target_sources})
  target_link_libraries(gpuview_capture_target PRIVATE d3d11 dxgi)
  set_target_properties(gpuview_capture_target PROPERTIES AUTOMOC OFF)
endif()

gpuview_collect_sources(frame_benchmark_sources tools/frame_benchmark)
qt_add_executable(gpuview_frame_benchmark ${frame_benchmark_sources})
target_link_libraries(gpuview_frame_benchmark PRIVATE gpuview_widgets)
if(WIN32)
  target_link_libraries(gpuview_frame_benchmark PRIVATE psapi)
endif()
