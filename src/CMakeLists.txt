# Core RDS encoding library
add_library(rds STATIC
    rds.c
    rds_strings.c
    waveforms.c
)
target_link_libraries(rds PRIVATE m)

# FM MPX stereo and baseband generator with built-in zero-dep WAV I/O
add_library(fm_mpx STATIC
    fm_mpx.c
    wav_io.c
)
target_link_libraries(fm_mpx PUBLIC rds m)

# Hardware abstraction layer (Raspberry Pi DMA / VideoCore Mailbox)
add_library(hw_rpi STATIC
    hw_rpi.c
    mailbox.c
)
target_link_libraries(hw_rpi PRIVATE m)

# Control server (UNIX socket & FIFO pipe)
add_library(control STATIC
    control.c
)

# Standalone baseband WAV exporter
add_executable(rds_wav
    rds_wav.c
)
target_link_libraries(rds_wav PRIVATE fm_mpx rds m)

# Main transmitter binary
add_executable(pi_fm_rds
    main.c
)
target_link_libraries(pi_fm_rds PRIVATE
    hw_rpi
    fm_mpx
    rds
    control
    m
)

# Convenience alias / symlink target
add_custom_target(pifm-enchanted ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink pi_fm_rds pifm-enchanted
    DEPENDS pi_fm_rds
)
