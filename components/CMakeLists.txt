# Auto-discover and add all subdirectories in this lib folder

# Gather immediate subdirectories
file(GLOB children RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*/")
foreach(child ${children})
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${child}/CMakeLists.txt)
        add_subdirectory(${child})
    endif()
endforeach()
