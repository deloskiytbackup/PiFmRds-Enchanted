add_executable(test_rds_crc test_rds_crc.c)
target_link_libraries(test_rds_crc PRIVATE rds m)
add_test(NAME test_rds_crc COMMAND test_rds_crc)

add_executable(test_rds_strings test_rds_strings.c)
target_link_libraries(test_rds_strings PRIVATE rds m)
add_test(NAME test_rds_strings COMMAND test_rds_strings)

add_executable(test_rds_groups test_rds_groups.c)
target_link_libraries(test_rds_groups PRIVATE rds m)
add_test(NAME test_rds_groups COMMAND test_rds_groups)
