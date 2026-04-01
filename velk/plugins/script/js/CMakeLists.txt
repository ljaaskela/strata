# QuickJS-ng as internal object library (symbols stay inside velk_js.dll)
add_library(quickjs_obj OBJECT
    third_party/quickjs/quickjs.c
    third_party/quickjs/dtoa.c
    third_party/quickjs/libregexp.c
    third_party/quickjs/libunicode.c
)

set_target_properties(quickjs_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(quickjs_obj PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/quickjs
)

target_compile_definitions(quickjs_obj PRIVATE
    _GNU_SOURCE
)

if(WIN32)
    target_compile_definitions(quickjs_obj PRIVATE
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0601
    )
endif()

if(MSVC)
    target_compile_options(quickjs_obj PRIVATE
        /wd4244 /wd4267 /wd4018 /wd4146 /wd4334 /wd4996 /wd4090
        /wd4200 /wd4805 /wd4804 /wd4100 /wd4245 /wd4456 /wd4457
    )
    target_compile_definitions(quickjs_obj PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
else()
    target_compile_options(quickjs_obj PRIVATE -w)
endif()

find_package(Threads)

# JS scripting plugin
add_library(velk_js SHARED
    src/js_plugin.h
    src/js_plugin.cpp
    src/js_context.h
    src/js_context.cpp
    src/js_proxy.h
    src/js_proxy.cpp
    src/js_marshal.h
    src/js_marshal.cpp
    src/js_any.h
    src/js_any.cpp
    src/js_function.h
    src/js_function.cpp
    src/js_import_handler.h
    src/js_import_handler.cpp
    include/velk/plugins/script/js/plugin.h
    include/velk/plugins/script/js/interface/intf_js_plugin.h
    $<TARGET_OBJECTS:quickjs_obj>
)

target_link_libraries(velk_js PRIVATE velk ${CMAKE_THREAD_LIBS_INIT})

target_include_directories(velk_js
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/quickjs
)

target_compile_definitions(velk_js PRIVATE VELK_EXPORTS)

set_target_properties(velk_js PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)
