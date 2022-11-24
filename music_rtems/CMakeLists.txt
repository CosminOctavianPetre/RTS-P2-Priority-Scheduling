add_executable(${TARGET_RTEMS1})
target_sources(${TARGET_RTEMS1}
		PRIVATE music_rtems_part1.c
)

# set required compiler and linker flags
target_compile_options(${TARGET_RTEMS1} PRIVATE ${compiler_flags})
target_link_options(${TARGET_RTEMS1} PRIVATE ${linker_flags})

add_executable(${TARGET_RTEMS2})
target_sources(${TARGET_RTEMS2}
		PRIVATE music_rtems_part2.c
)

# set required compiler and linker flags
target_compile_options(${TARGET_RTEMS2} PRIVATE ${compiler_flags})
target_link_options(${TARGET_RTEMS2} PRIVATE ${linker_flags})
