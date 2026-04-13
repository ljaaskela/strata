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

# TRACY_NO_SYSTEM_TRACING: disable ETW tracing (needs newer Windows SDK than MSVC 2019 provides)
# RelationProcessorDie=5: enum value missing from older Windows SDK headers
target_compile_definitions(velk_tracy PRIVATE
    VELK_EXPORTS
    TRACY_ENABLE
    TRACY_NO_SYSTEM_TRACING
    "RelationProcessorDie=static_cast<LOGICAL_PROCESSOR_RELATIONSHIP>(5)"
)

# Tracy needs exceptions enabled (velk core disables them)
if(MSVC)
    target_compile_options(velk_tracy PRIVATE /EHsc)
endif()
