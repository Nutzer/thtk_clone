find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)
add_definitions( -g )
bison_target(AnsParse ansparse.y ${CMAKE_CURRENT_BINARY_DIR}/ansparse.c)
flex_target(AnsScan ansscan.l ${CMAKE_CURRENT_BINARY_DIR}/ansscan.c)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
add_executable(thanmx
  ${BISON_AnsParse_OUTPUT_SOURCE} ${FLEX_AnsScan_OUTPUTS}
   eclmap.c eclmap.h eclarg.c eclarg.h thanm.c image.c thanm.h image.h anmx_parse.c anmx_parse.h
  )
if(WIN32)
    set(ADDITIONAL_LIBRARIES png)
else()
    set(ADDITIONAL_LIBRARIES m png)
endif()
target_link_libraries(thanmx util ${ADDITIONAL_LIBRARIES})
link_setargv(thanmx)
install(TARGETS thanmx DESTINATION bin)
install(FILES thanmx.1 DESTINATION share/man/man1)
