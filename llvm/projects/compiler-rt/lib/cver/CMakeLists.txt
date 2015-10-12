# Build for the undefined behavior sanitizer runtime support library.

set(CVER_SOURCES
  cver_common.cc
  cver_posix.cc
  cver_init.cc
  cver_new_delete.cc
  cver_malloc.cc
  cver_allocator.cc
  cver_rbtree.cc
  cver_thread.cc
  cver_flags.cc
  cver_report.cc
  cver_stats.cc
  )

include_directories(..)

set(CVER_CFLAGS ${SANITIZER_COMMON_CFLAGS})
append_no_rtti_flag(CVER_CFLAGS)

add_custom_target(cver)

if(APPLE)
  # Build universal binary on APPLE.
  add_compiler_rt_osx_static_runtime(clang_rt.cver_osx
    ARCH ${CVER_SUPPORTED_ARCH}
    SOURCES ${CVER_SOURCES}
            $<TARGET_OBJECTS:RTSanitizerCommon.osx>
    CFLAGS ${CVER_CFLAGS})
  add_dependencies(cver clang_rt.cver_osx)
else()
  # Build separate libraries for each target.
  foreach(arch ${CVER_SUPPORTED_ARCH})
    # Main Cver runtime.
    add_compiler_rt_runtime(clang_rt.cver-${arch} ${arch} STATIC
      SOURCES ${CVER_SOURCES}
      CFLAGS ${CVER_CFLAGS})
    add_dependencies(cver
      clang_rt.san-${arch}
      clang_rt.cver-${arch})
    if (UNIX AND NOT ${arch} STREQUAL "i386")
      add_sanitizer_rt_symbols(clang_rt.cver-${arch} cver.syms.extra)
      add_dependencies(cver
        clang_rt.cver-${arch}-symbols)
    endif()
  endforeach()
endif()

add_dependencies(compiler-rt cver)
