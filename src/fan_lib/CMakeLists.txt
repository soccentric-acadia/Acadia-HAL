add_library(fan_lib fan_lib.cpp fan_lib.h)
target_include_directories(fan_lib PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fan_lib PUBLIC lib_base)
install(TARGETS fan_lib
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES fan_lib.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/acadia_hal)
