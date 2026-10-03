# cmake support for quickjs by LibreSprite contributors

add_library(quickjs quickjs-amalgam.c)
set_target_properties(quickjs PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
if(VITA)
  # The std/os modules need a full POSIX environment (termios, fork...);
  # provide only the few helpers the interpreter uses.
  target_sources(quickjs PRIVATE quickjs-libc-min.c)
else()
  target_compile_definitions(quickjs PUBLIC QJS_BUILD_LIBC)
endif()
target_include_directories(quickjs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
