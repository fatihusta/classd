cmake_minimum_required(VERSION 3.1.0)
project(classd)

# debug: list all available cmake properties
get_cmake_property(_variableNames VARIABLES)
list (SORT _variableNames)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()

# find the appropriate libnavl, and link against it
set(LIB_NAVL navl)
if ("${CMAKE_LIBRARY_ARCHITECTURE}" STREQUAL "") # OpenWrt
  if (CMAKE_C_FLAGS MATCHES "cortex-a9")
    set(LIB_NAVL_DIR ./src/vineyard/lib-mvebu-openwrt-musl)
  elseif (CMAKE_C_FLAGS MATCHES "cortex-a53")
    set(LIB_NAVL_DIR ./src/vineyard/lib-aarch64-openwrt-musl)
  else ()
    set(LIB_NAVL_DIR ./src/vineyard/lib-x86_64-openwrt-musl)
  endif (CMAKE_C_FLAGS MATCHES "cortex-a9")
else () # Debian
  set(LIB_NAVL_DIR ./src/vineyard/lib-${CMAKE_LIBRARY_ARCHITECTURE})
  # also add -lpthread and -ldl
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  set(EXTRA_LIBS Threads::Threads ${CMAKE_DL_LIBS})
endif ("${CMAKE_LIBRARY_ARCHITECTURE}" STREQUAL "")

# main executable
file(GLOB SOURCES "src/*.cpp")
link_directories(${LIB_NAVL_DIR})
add_executable(classd ${SOURCES})
target_link_libraries(classd ${LIB_NAVL} ${EXTRA_LIBS})

# installable files
file(GLOB STATIC "files/*")
file(GLOB LIBS "${LIB_NAVL_DIR}/*")

# install targets
install(TARGETS classd RUNTIME DESTINATION $ENV{DESTDIR}/usr/bin)
install(FILES ${LIBS} DESTINATION $ENV{DESTDIR}/usr/lib)
install(DIRECTORY ${STATIC} DESTINATION $ENV{DESTDIR}/)
