# 按模块目录收集源码；新增/删除文件会在构建时触发CMake重新配置。
# 不扫描整个仓库，避免不同可执行程序的main和测试入口混在同一个目标。
function(gpuview_collect_sources output directory)
  file(GLOB_RECURSE module_files CONFIGURE_DEPENDS
    RELATIVE "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/${directory}/*.cpp"
    "${PROJECT_SOURCE_DIR}/${directory}/*.cxx"
    "${PROJECT_SOURCE_DIR}/${directory}/*.h"
    "${PROJECT_SOURCE_DIR}/${directory}/*.hpp"
    "${PROJECT_SOURCE_DIR}/${directory}/*.ui"
    "${PROJECT_SOURCE_DIR}/${directory}/*.qrc")
  if(NOT module_files)
    message(FATAL_ERROR "No source files found in ${directory}")
  endif()
  set(group_files)
  foreach(source IN LISTS module_files)
    list(APPEND group_files "${PROJECT_SOURCE_DIR}/${source}")
  endforeach()
  source_group(TREE "${PROJECT_SOURCE_DIR}" FILES ${group_files})
  set(${output} ${module_files} PARENT_SCOPE)
endfunction()
