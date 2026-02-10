cmake_minimum_required(VERSION 3.5)

project(strata LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/Zc:preprocessor)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(strata SHARED
    src/property.cpp
    src/property.h
    src/event.cpp
    src/event.h
    src/function.cpp
    src/function.h
    src/strata_export.h
    array_view.h
    common.h
    src/metadata_container.cpp
    src/metadata_container.h
    src/strata_impl.cpp
    src/strata_impl.h
    src/strata.cpp
    interface/intf_any.h
    interface/intf_external_any.h
    interface/intf_object.h
    interface/intf_interface.h
    interface/refcnt_ptr.h
    interface/intf_function.h
    interface/intf_event.h
    interface/intf_property.h
    interface/intf_object_factory.h
    interface/intf_strata.h
    interface/intf_metadata.h
    interface/types.h
    api/any.h
    api/property.h
    api/function.h
    api/strata.h
    ext/any.h
    ext/core_object.h
    ext/metadata.h
    ext/object.h
    ext/event.h
    )

target_compile_definitions(strata PRIVATE STRATA_EXPORTS)
