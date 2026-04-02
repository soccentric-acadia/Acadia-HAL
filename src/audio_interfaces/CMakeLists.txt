add_library(audio_interfaces audio_interfaces.cpp audio_interfaces.h)
target_include_directories(audio_interfaces PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(audio_interfaces lib_base)
