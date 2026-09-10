# Core RDS encoding library
add_library(rds STATIC
    rds.c
    rds_strings.c
    waveforms.c
)
target_link_libraries(rds PRIVATE m)

# Broadcast Audio DSP & MPX stereo generator
add_library(fm_mpx STATIC
    fm_mpx.c
    dsp_processor.c
    wav_io.c
)
target_link_libraries(fm_mpx PUBLIC rds m)

# SDR I/Q Modulator library
add_library(iq_modulator STATIC
    iq_modulator.c
)
target_link_libraries(iq_modulator PUBLIC m)

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

# App Configuration file parser
add_library(config_file STATIC
    config_file.c
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
    iq_modulator
    rds
    control
    config_file
    m
)

# Convenience alias / symlink target: pifm and pifm-enchanted
add_custom_target(pifm-aliases ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink pi_fm_rds pifm
    COMMAND ${CMAKE_COMMAND} -E create_symlink pi_fm_rds pifm-enchanted
    DEPENDS pi_fm_rds
)
