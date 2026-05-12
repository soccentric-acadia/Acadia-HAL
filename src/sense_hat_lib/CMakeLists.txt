add_library(sense_hat_lib sense_hat_lib.cpp sense_hat_lib.h)
target_include_directories(sense_hat_lib PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sense_hat_lib lib_base)
