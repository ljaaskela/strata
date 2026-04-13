# =============================================================================
# Tracy: extract vendored tarball
# =============================================================================
set(TRACY_EXTRACT_DIR "${CMAKE_BINARY_DIR}/_deps/tracy-src")
if(NOT EXISTS "${TRACY_EXTRACT_DIR}/TracyClient.cpp")
    file(MAKE_DIRECTORY "${TRACY_EXTRACT_DIR}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tracy-0.13.1.tar.gz"
        WORKING_DIRECTORY "${TRACY_EXTRACT_DIR}"
    )
endif()

# =============================================================================
# Tracy plugin
# =============================================================================
add_library(velk_tracy SHARED
    src/tracy_plugin.h
    src/tracy_plugin.cpp
    src/tracy_perf_sink.h
    src/tracy_perf_sink.cpp
    include/velk/plugins/tracy/plugin.h
    "${TRACY_EXTRACT_DIR}/TracyClient.cpp"
)

target_link_libraries(velk_tracy PRIVATE velk)

target_include_directories(velk_tracy
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${TRACY_EXTRACT_DIR}
)

target_compile_definitions(velk_tracy PRIVATE
    VELK_EXPORTS
    TRACY_ENABLE
    TRACY_NO_SYSTEM_TRACING
)

# RelationProcessorDie was added in Windows SDK 10.0.20348. Define it only when missing.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_QUIET ON)
check_cxx_source_compiles("
    #include <windows.h>
    int main() { auto x = RelationProcessorDie; return (int)x; }
" HAS_RELATION_PROCESSOR_DIE)
if(NOT HAS_RELATION_PROCESSOR_DIE)
    target_compile_definitions(velk_tracy PRIVATE
        "RelationProcessorDie=static_cast<LOGICAL_PROCESSOR_RELATIONSHIP>(5)"
    )
endif()

# Tracy needs exceptions enabled (velk core disables them)
if(MSVC)
    target_compile_options(velk_tracy PRIVATE /EHsc)
endif()
