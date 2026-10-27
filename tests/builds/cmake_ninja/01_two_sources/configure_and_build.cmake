# Configures the case and builds it in one run of CMake.
execute_process(COMMAND ${CMAKE_COMMAND} -S . -B build -G Ninja OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND ${CMAKE_COMMAND} --build build -j 1 COMMAND_ERROR_IS_FATAL ANY)
