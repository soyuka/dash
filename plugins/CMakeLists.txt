function(dash_add_plugin type name)
    set(target dash_plugin_${type}_${name})
    add_library(${target} MODULE ${ARGN})
    target_link_libraries(${target} PRIVATE dash_core)
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME ${name}
        LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/${DASH_PLUGIN_INSTALL_DIR}/${type}
        INSTALL_RPATH "$ORIGIN/../../..")
    install(TARGETS ${target} LIBRARY DESTINATION ${DASH_PLUGIN_INSTALL_DIR}/${type})
endfunction()

# Drop-in layout kept from upstream: every plugins/<type>/<name>/ directory
# becomes plugin <name> of <type>, built from the .cpp files it contains.
file(GLOB plugin_types LIST_DIRECTORIES true RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)
foreach(type IN LISTS plugin_types)
    if(NOT IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${type})
        continue()
    endif()
    file(GLOB names LIST_DIRECTORIES true RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/${type} ${CMAKE_CURRENT_SOURCE_DIR}/${type}/*)
    foreach(name IN LISTS names)
        file(GLOB_RECURSE sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${type}/${name}/*.cpp)
        if(sources)
            dash_add_plugin(${type} ${name} ${sources})
        endif()
    endforeach()
endforeach()
