cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
# --- custom targets: ---
INCLUDE( cmake/Modules/TargetDistclean.cmake OPTIONAL)

project (CLAMR)
# The version number.
set (CLAMR_VERSION_MAJOR 2)
set (CLAMR_VERSION_MINOR 0.7)
set (CLAMR_VERSION "${CLAMR_VERSION_MAJOR}.${CLAMR_VERSION_MINOR}")
 
if (NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif(NOT CMAKE_BUILD_TYPE)

set(CMAKE_C_FLAGS "-std=c99")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g")

find_package(OpenCL REQUIRED)
#  OPENCL_FOUND        : TRUE if found, FALSE otherwise
#  OPENCL_INCLUDE_DIRS : Include directories for OpenCL
#  OPENCL_LIBRARIES    : The libraries to link against
if (OPENCL_FOUND)
   include_directories(${OPENCL_INCLUDE_DIRS})
endif (OPENCL_FOUND)
find_package(MPI REQUIRED)
if (MPI_FOUND)
   include_directories(${MPI_INCLUDE_PATH})
endif (MPI_FOUND)

find_package(OpenGL)
find_package(MPE)
if (OPENGL_FOUND)
   set (HAVE_GRAPHICS on)
   set (HAVE_OPENGL on)
endif (OPENGL_FOUND)
if (NOT OPENGL_FOUND)
   if (MPE_FOUND)
      set (HAVE_GRAPHICS on)
      set (HAVE_MPE on)
   endif (MPE_FOUND)
endif (NOT OPENGL_FOUND)

# configure a header file to pass some of the CMake settings
# to the source code
configure_file ( "${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_BINARY_DIR}/config.h")
 
add_definitions(-DHAVE_CONFIG_H)

# add the binary tree to the search path for include files
# so that we will find config.h
include_directories("${PROJECT_BINARY_DIR}")

add_subdirectory(l7)
add_subdirectory(kdtree)
add_subdirectory(hsfc)
add_subdirectory(s7)
add_subdirectory(timer)
add_subdirectory(ezcl)
add_subdirectory(zorder)
add_subdirectory(genmalloc)
add_subdirectory(MallocPlus)

########### clamr target ###############

set(clamr_CXX_SRCS clamr.cpp mesh.cpp partition.cpp state.cpp input.cpp)
set(clamr_C_SRCS display.c reduce.c)
set(clamr_H_SRCS display.h mesh.h partition.h reorder.h state.h input.h reduce.h)
#clamr_LDADD hsfc/libhsfc.la kdtree/libkdtree.la zorder/libzorder.la s7/libs7.la ezcl/libpezcl.la timer/libtimer.la l7/libl7.la genmalloc/libgenmalloc.la MallocPlus/libMallocPlus.la -lm ${MPE_LIBS} ${MPI_LIBS} ${OPENCL_LIBS})

set(clamr_SRCS ${clamr_CXX_SRCS} ${clamr_C_SRCS} ${clamr_H_SRCS})

add_executable(clamr ${clamr_SRCS})

set_target_properties(clamr PROPERTIES COMPILE_DEFINITIONS HAVE_MPI_AND_OPENCL)

target_link_libraries(clamr hsfc kdtree zorder s7 pezcl timer l7 genmalloc MallocPlus m)
target_link_libraries(clamr ${MPI_LIBRARIES})
target_link_libraries(clamr ${OPENCL_LIBRARIES})

install(TARGETS clamr DESTINATION bin)

########### clamr_gpuonly target ###############
set(clamr_gpuonly_CXX_SRCS clamr_gpuonly.cpp mesh.cpp partition.cpp state.cpp input.cpp)
set(clamr_gpuonly_C_SRCS display.c reduce.c)
set(clamr_gpuonly_H_SRCS display.h mesh.h partition.h reorder.h state.h input.h reduce.h)
#clamr_gpuonly_LDADD = hsfc/libhsfc.la kdtree/libkdtree.la zorder/libzorder.la s7/libs7.la ezcl/libezcl.la timer/libtimer.la genmalloc/libgenmalloc.la MallocPlus/libMallocPlus.la -lm ${MPE_NOMPI_LIBS} ${OPENCL_LIBS}

set(clamr_gpuonly_SRCS ${clamr_gpuonly_CXX_SRCS} ${clamr_gpuonly_C_SRCS} ${clamr_gpuonly_H_SRCS})

add_executable(clamr_gpuonly ${clamr_gpuonly_SRCS})

set_target_properties(clamr_gpuonly PROPERTIES COMPILE_DEFINITIONS HAVE_OPENCL)

target_link_libraries(clamr_gpuonly hsfc kdtree zorder s7 ezcl timer genmalloc MallocPlus m)
target_link_libraries(clamr_gpuonly ${OPENCL_LIBRARIES})

install(TARGETS clamr_gpuonly DESTINATION bin)

########### clamr_cpuonly target ###############

set(clamr_cpuonly_CXX_SRCS clamr_cpuonly.cpp mesh.cpp partition.cpp state.cpp input.cpp)
set(clamr_cpuonly_C_SRCS display.c)
set(clamr_cpuonly_H_SRCS display.h mesh.h partition.h reorder.h state.h input.h)
#clamr_cpuonly_LDADD = hsfc/libhsfc.la kdtree/libkdtree.la zorder/libzorder.la s7/libs7.la timer/libtimer.la genmalloc/libgenmalloc.la MallocPlus/libMallocPlus.la -lm ${MPE_NOMPI_LIBS}

set(clamr_cpuonly_SRCS ${clamr_cpuonly_CXX_SRCS} ${clamr_cpuonly_C_SRCS} ${clamr_cpuonly_H_SRCS})

add_executable(clamr_cpuonly ${clamr_cpuonly_SRCS})

target_link_libraries(clamr_cpuonly hsfc kdtree zorder s7 timer genmalloc MallocPlus m)

install(TARGETS clamr_cpuonly DESTINATION bin)

########### clamr_mpionly target ###############
set(clamr_mpionly_CXX_SRCS clamr_mpionly.cpp mesh.cpp partition.cpp state.cpp input.cpp)
set(clamr_mpionly_C_SRCS display.c)
set(clamr_mpionly_H_SRCS display.h mesh.h partition.h reorder.h state.h input.h)
#clamr_mpionly_LDADD = hsfc/libhsfc.la kdtree/libkdtree.la zorder/libzorder.la s7/libs7.la timer/libtimer.la l7/libl7.la genmalloc/libgenmalloc.la MallocPlus/libMallocPlus.la -lm ${MPE_LIBS} ${MPI_LIBS}

set(clamr_mpionly_SRCS ${clamr_mpionly_CXX_SRCS} ${clamr_mpionly_C_SRCS} ${clamr_mpionly_H_SRCS})

add_executable(clamr_mpionly ${clamr_mpionly_SRCS})

set_target_properties(clamr_mpionly PROPERTIES COMPILE_DEFINITIONS HAVE_MPI)

target_link_libraries(clamr_mpionly hsfc kdtree zorder s7 timer l7 genmalloc MallocPlus m)
target_link_libraries(clamr_mpionly ${MPI_LIBRARIES})

install(TARGETS clamr_mpionly DESTINATION bin)

########### clamr_gpucheck target ###############
set(clamr_gpucheck_CXX_SRCS clamr_gpucheck.cpp mesh.cpp partition.cpp state.cpp input.cpp)
set(clamr_gpucheck_C_SRCS display.c reduce.c)
set(clamr_gpucheck_H_SRCS display.h mesh.h partition.h reorder.h state.h input.h reduce.h)
#clamr_gpucheck_LDADD = hsfc/libhsfc.la kdtree/libkdtree.la zorder/libzorder.la s7/libs7.la ezcl/libezcl.la timer/libtimer.la genmalloc/libgenmalloc.la MallocPlus/libMallocPlus.la -lm ${MPE_NOMPI_LIBS} ${OPENCL_LIBS}

set(clamr_gpucheck_SRCS ${clamr_gpucheck_CXX_SRCS} ${clamr_gpucheck_C_SRCS} ${clamr_gpucheck_H_SRCS})

add_executable(clamr_gpucheck ${clamr_gpucheck_SRCS})

set_target_properties(clamr_gpucheck PROPERTIES COMPILE_DEFINITIONS HAVE_OPENCL)

target_link_libraries(clamr_gpucheck hsfc kdtree zorder s7 ezcl timer genmalloc MallocPlus m)
target_link_libraries(clamr_gpucheck ${OPENCL_LIBRARIES})

install(TARGETS clamr_gpucheck DESTINATION bin)

########### clamr_mpicheck target ###############
set(clamr_mpicheck_CXX_SRCS clamr_mpicheck.cpp mesh.cpp partition.cpp state.cpp input.cpp)
set(clamr_mpicheck_C_SRCS display.c)
set(clamr_mpicheck_H_SRCS display.h mesh.h partition.h reorder.h state.h input.h)
#clamr_mpicheck_LDADD = hsfc/libhsfc.la kdtree/libkdtree.la zorder/libzorder.la s7/libs7.la timer/libtimer.la l7/libl7.la genmalloc/libgenmalloc.la MallocPlus/libMallocPlus.la -lm ${MPE_LIBS} ${MPI_LIBS}

set(clamr_mpicheck_SRCS ${clamr_mpicheck_CXX_SRCS} ${clamr_mpicheck_C_SRCS} ${clamr_mpicheck_H_SRCS})

add_executable(clamr_mpicheck ${clamr_mpicheck_SRCS})

set_target_properties(clamr_mpicheck PROPERTIES COMPILE_DEFINITIONS HAVE_MPI)

target_link_libraries(clamr_mpicheck hsfc kdtree zorder s7 timer l7 genmalloc MallocPlus m)
target_link_libraries(clamr_mpicheck ${MPI_LIBRARIES})

install(TARGETS clamr_mpicheck DESTINATION bin)

########### clamr_checkall target ###############
set(clamr_checkall_CXX_SRCS clamr_checkall.cpp mesh.cpp partition.cpp state.cpp input.cpp)
set(clamr_checkall_C_SRCS display.c reduce.c)
set(clamr_checkall_H_SRCS display.h mesh.h partition.h reorder.h state.h input.h reduce.h)
#clamr_checkall_LDADD = hsfc/libhsfc.la kdtree/libkdtree.la zorder/libzorder.la s7/libs7.la ezcl/libpezcl.la timer/libtimer.la l7/libl7.la genmalloc/libgenmalloc.la MallocPlus/libMallocPlus.la -lm ${MPE_LIBS} ${MPI_LIBS} ${OPENCL_LIBS}

set(clamr_checkall_SRCS ${clamr_checkall_CXX_SRCS} ${clamr_checkall_C_SRCS} ${clamr_checkall_H_SRCS})

add_executable(clamr_checkall ${clamr_checkall_SRCS})

set_target_properties(clamr_checkall PROPERTIES COMPILE_DEFINITIONS HAVE_MPI_AND_OPENCL)

target_link_libraries(clamr_checkall hsfc kdtree zorder s7 pezcl timer l7 genmalloc MallocPlus m)
target_link_libraries(clamr_checkall ${MPI_LIBRARIES})
target_link_libraries(clamr_checkall ${OPENCL_LIBRARIES})

install(TARGETS clamr_checkall DESTINATION bin)

########### install files ###############


#========== original Makefile.am contents follow ===========

#bin_PROGRAMS = clamr clamr_gpuonly clamr_cpuonly clamr_mpionly clamr_gpucheck clamr_mpicheck clamr_checkall
#
#SUBDIRS = l7 kdtree hsfc s7 timer ezcl zorder genmalloc MallocPlus
#
#AM_MAKEFLAGS = -j
#
#ACLOCAL_AMFLAGS = -I config
#
## -all-static flag is to make debugging easier. Removing it will enable dynamic libraries instead
#
#clamr_CXX_SRCS = clamr.cpp mesh.cpp partition.cpp state.cpp input.cpp
#clamr_C_SRCS = display.c reduce.c
#clamr_H_SRCS = display.h mesh.h partition.h reorder.h state.h input.h reduce.h
#clamr_CPPFLAGS = -DHAVE_MPI=1 -DHAVE_OPENCL=1
#clamr_LDADD = hsfc/libhsfc.la kdtree/libkdtree.la zorder/libzorder.la s7/libs7.la ezcl/libpezcl.la timer/libtimer.la l7/libl7.la genmalloc/libgenmalloc.la MallocPlus/libMallocPlus.la -lm ${MPE_LIBS} ${MPI_LIBS} ${OPENCL_LIBS}
#
#clamr_SOURCES = ${clamr_CXX_SRCS} ${clamr_C_SRCS} ${clamr_H_SRCS}
#
#clamr_gpuonly_CXX_SRCS = clamr_gpuonly.cpp mesh.cpp partition.cpp state.cpp input.cpp
#clamr_gpuonly_C_SRCS = display.c reduce.c
#clamr_gpuonly_H_SRCS = display.h mesh.h partition.h reorder.h state.h input.h reduce.h
#clamr_gpuonly_CPPFLAGS = -UHAVE_MPI -DHAVE_OPENCL=1
#clamr_gpuonly_LDADD = hsfc/libhsfc.la kdtree/libkdtree.la zorder/libzorder.la s7/libs7.la ezcl/libezcl.la timer/libtimer.la genmalloc/libgenmalloc.la MallocPlus/libMallocPlus.la -lm ${MPE_NOMPI_LIBS} ${OPENCL_LIBS}
#
#clamr_gpuonly_SOURCES = ${clamr_gpuonly_CXX_SRCS} ${clamr_gpuonly_C_SRCS} ${clamr_gpuonly_H_SRCS}
#
#clamr_cpuonly_CXX_SRCS = clamr_cpuonly.cpp mesh.cpp partition.cpp state.cpp input.cpp
#clamr_cpuonly_C_SRCS = display.c
#clamr_cpuonly_H_SRCS = display.h mesh.h partition.h reorder.h state.h input.h
#clamr_cpuonly_CPPFLAGS = -UHAVE_MPI -UHAVE_OPENCL
#clamr_cpuonly_LDADD = hsfc/libhsfc.la kdtree/libkdtree.la zorder/libzorder.la s7/libs7.la timer/libtimer.la genmalloc/libgenmalloc.la MallocPlus/libMallocPlus.la -lm ${MPE_NOMPI_LIBS}
#
#clamr_cpuonly_SOURCES = ${clamr_cpuonly_CXX_SRCS} ${clamr_cpuonly_C_SRCS} ${clamr_cpuonly_H_SRCS}
#
#clamr_mpionly_CXX_SRCS = clamr_mpionly.cpp mesh.cpp partition.cpp state.cpp input.cpp
#clamr_mpionly_C_SRCS = display.c
#clamr_mpionly_H_SRCS = display.h mesh.h partition.h reorder.h state.h input.h
#clamr_mpionly_CPPFLAGS = -DHAVE_MPI=1 -UHAVE_OPENCL
#clamr_mpionly_LDADD = hsfc/libhsfc.la kdtree/libkdtree.la zorder/libzorder.la s7/libs7.la timer/libtimer.la l7/libl7.la genmalloc/libgenmalloc.la MallocPlus/libMallocPlus.la -lm ${MPE_LIBS} ${MPI_LIBS}
#
#clamr_mpionly_SOURCES = ${clamr_mpionly_CXX_SRCS} ${clamr_mpionly_C_SRCS} ${clamr_mpionly_H_SRCS}
#
#clamr_mpicheck_CXX_SRCS = clamr_mpicheck.cpp mesh.cpp partition.cpp state.cpp input.cpp
#clamr_mpicheck_C_SRCS = display.c
#clamr_mpicheck_H_SRCS = display.h mesh.h partition.h reorder.h state.h input.h
#clamr_mpicheck_CPPFLAGS = -DHAVE_MPI=1 -UHAVE_OPENCL
#clamr_mpicheck_LDADD = hsfc/libhsfc.la kdtree/libkdtree.la zorder/libzorder.la s7/libs7.la timer/libtimer.la l7/libl7.la genmalloc/libgenmalloc.la MallocPlus/libMallocPlus.la -lm ${MPE_LIBS} ${MPI_LIBS}
#
#clamr_mpicheck_SOURCES = ${clamr_mpicheck_CXX_SRCS} ${clamr_mpicheck_C_SRCS} ${clamr_mpicheck_H_SRCS}
#
#clamr_gpucheck_CXX_SRCS = clamr_gpucheck.cpp mesh.cpp partition.cpp state.cpp input.cpp
#clamr_gpucheck_C_SRCS = display.c reduce.c
#clamr_gpucheck_H_SRCS = display.h mesh.h partition.h reorder.h state.h input.h reduce.h
#clamr_gpucheck_CPPFLAGS = -UHAVE_MPI -DHAVE_OPENCL=1
#clamr_gpucheck_LDADD = hsfc/libhsfc.la kdtree/libkdtree.la zorder/libzorder.la s7/libs7.la ezcl/libezcl.la timer/libtimer.la genmalloc/libgenmalloc.la MallocPlus/libMallocPlus.la -lm ${MPE_NOMPI_LIBS} ${OPENCL_LIBS}
#
#clamr_gpucheck_SOURCES = ${clamr_gpucheck_CXX_SRCS} ${clamr_gpucheck_C_SRCS} ${clamr_gpucheck_H_SRCS}
#
#clamr_checkall_CXX_SRCS = clamr_checkall.cpp mesh.cpp partition.cpp state.cpp input.cpp
#clamr_checkall_C_SRCS = display.c reduce.c 
#clamr_checkall_H_SRCS = display.h mesh.h partition.h reorder.h state.h input.h reduce.h
#clamr_checkall_CPPFLAGS = -DHAVE_MPI=1 -DHAVE_OPENCL=1
#clamr_checkall_LDADD = hsfc/libhsfc.la kdtree/libkdtree.la zorder/libzorder.la s7/libs7.la ezcl/libpezcl.la timer/libtimer.la l7/libl7.la genmalloc/libgenmalloc.la MallocPlus/libMallocPlus.la -lm ${MPE_LIBS} ${MPI_LIBS} ${OPENCL_LIBS}
#
#clamr_checkall_SOURCES = ${clamr_checkall_CXX_SRCS} ${clamr_checkall_C_SRCS} ${clamr_checkall_H_SRCS}
#
#EXTRA_DIST = wave_kern.cl wave_kern_calc.cl reduce.cl config.h clamr.xcodeproj/project.pbxproj clamr.1 fileamr.in AUTHORS ChangeLog runtests.sh newbatch.sh LICENSE
#
#l7/libl7.la:
#	cd l7; make
#
#hsfc/libhsfc.la:
#	cd hsfc; make
#
#kdtree/libkdtree.la:
#	cd kdtree; make
#
#zorder/libzorder.la:
#	cd zorder; make
#
#s7/libs7.la:
#	cd s7; make
#
#ezcl/libezcl.la:
#	cd ezcl; make libezcl.la
#
#ezcl/libpezcl.la:
#	cd ezcl; make libpezcl.la
#
#timer/libtimer.la:
#	cd timer; make
#
#genmalloc/libgenmalloc.la:
#	cd genmalloc; make
#
#MallocPlus/libMallocPlus.la:
#	cd MallocPlus; make
#
#distclean-local:
#	rm -f config.h.in~
#	rm -rf autom4te.cache
#	rm -rf config.log config.status