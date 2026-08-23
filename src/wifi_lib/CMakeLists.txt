add_library(wifi_lib wifi_lib.cpp wifi_lib.h)
target_include_directories(wifi_lib PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wifi_lib PUBLIC lib_base)
install(TARGETS wifi_lib
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES wifi_lib.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/acadia_hal)
