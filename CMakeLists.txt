cmake_minimum_required(VERSION 3.1.0)
project(classd)

get_cmake_property(_variableNames VARIABLES)
list (SORT _variableNames)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()
 
set(LIB_NAVL navl)
set(LIB_NAVL_DIR ./src/vineyard/lib-${CMAKE_LIBRARY_ARCHITECTURE})
link_directories(${LIB_NAVL_DIR})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

file(GLOB SOURCES "src/*.cpp")
file(GLOB STATIC "files/*")
file(GLOB LIBS "${LIB_NAVL_DIR}/*")

add_executable(classd ${SOURCES})

if (CMAKE_C_FLAGS MATCHES "cortex-a9")
  target_link_libraries(classd ${LIB_NAVL})
else()
  target_link_libraries(classd ${LIB_NAVL} Threads::Threads ${CMAKE_DL_LIBS})
endif (CMAKE_C_FLAGS MATCHES "cortex-a9")

install(TARGETS classd RUNTIME DESTINATION $ENV{DESTDIR}/bin)
install(FILES ${LIBS} DESTINATION $ENV{DESTDIR}/usr/lib)
install(DIRECTORY ${STATIC} DESTINATION $ENV{DESTDIR}/)
