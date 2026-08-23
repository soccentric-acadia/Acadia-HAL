add_library(hdmi_lib hdmi_lib.cpp hdmi_lib.h)
target_include_directories(hdmi_lib PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hdmi_lib PUBLIC lib_base)
install(TARGETS hdmi_lib
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES hdmi_lib.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/acadia_hal)
