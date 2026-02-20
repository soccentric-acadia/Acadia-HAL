add_executable(test_storage main.cpp)
target_link_libraries(test_storage storage_lib)
target_include_directories(test_storage PRIVATE ../src/storage_lib)
add_test(NAME test_storage COMMAND test_storage)
