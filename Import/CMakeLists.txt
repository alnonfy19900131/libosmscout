set(OSMSCOUT_BUILD_IMPORT ON CACHE INTERNAL "" FORCE)

set(SOURCE_FILES
    src/Import.cpp
)

add_executable(Import ${SOURCE_FILES} ${HEADER_FILES})

set_target_properties(Import PROPERTIES CXX_STANDARD 14)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/Config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/include/Config.h)

target_include_directories(Import PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)

target_link_libraries(Import OSMScoutImport)

if(BUILD_IMPORT_TOOL_FOR_DISTRIBUTION AND (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX OR CMAKE_COMPILER_IS_GNUCC))
  set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
endif()

install(TARGETS Import
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
