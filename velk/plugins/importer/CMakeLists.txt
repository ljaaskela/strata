add_library(velk_importer SHARED
    src/importer_plugin.h
    src/importer_plugin.cpp
    include/velk/plugins/importer/interface/intf_importer_plugin.h
    include/velk/plugins/importer/plugin.h
)

target_link_libraries(velk_importer PRIVATE velk)

target_include_directories(velk_importer
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(velk_importer PRIVATE VELK_EXPORTS)
