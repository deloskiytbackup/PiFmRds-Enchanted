add_executable(test_rds_crc test_rds_crc.c)
target_link_libraries(test_rds_crc PRIVATE rds m)
add_test(NAME test_rds_crc COMMAND test_rds_crc)

add_executable(test_rds_strings test_rds_strings.c)
target_link_libraries(test_rds_strings PRIVATE rds m)
add_test(NAME test_rds_strings COMMAND test_rds_strings)

add_executable(test_rds_groups test_rds_groups.c)
target_link_libraries(test_rds_groups PRIVATE rds m)
add_test(NAME test_rds_groups COMMAND test_rds_groups)

add_executable(test_dsp_iq test_dsp_iq.c)
target_link_libraries(test_dsp_iq PRIVATE fm_mpx iq_modulator rds m)
add_test(NAME test_dsp_iq COMMAND test_dsp_iq)

add_executable(test_hifi_wav test_hifi_wav.c)
target_link_libraries(test_hifi_wav PRIVATE fm_mpx rds m)
add_test(NAME test_hifi_wav COMMAND test_hifi_wav)

