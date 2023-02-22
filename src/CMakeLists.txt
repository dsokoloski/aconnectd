add_executable(
  aconnectd
  main.cpp
)

install(TARGETS aconnectd
  DESTINATION ${CMAKE_INSTALL_PREFIX}/sbin
)

target_link_libraries(aconnectd ${ALSA_LIBRARIES})
target_include_directories(aconnectd PUBLIC ${ALSA_INCLUDE_DIRS})
target_compile_options(aconnectd PUBLIC ${ALSA_CFLAGS_OTHER})

