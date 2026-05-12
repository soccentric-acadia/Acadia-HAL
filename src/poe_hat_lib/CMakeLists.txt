add_library(poe_hat_lib poe_hat_lib.cpp poe_hat_lib.h)
target_include_directories(poe_hat_lib PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(poe_hat_lib lib_base)
