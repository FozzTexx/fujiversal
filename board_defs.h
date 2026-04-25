#ifndef BOARD_NAME
    #error "No board defined in CMake"
#else /* BOARD_NAME */
// Helper macros to turn tokens into strings
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

// To build the path for #include:
#define ROM_PATH STR(build/BOARD_NAME/rom.h)
#define PIO_HEADER STR(BOARD_NAME.pio.h)

// Computed include
#include ROM_PATH
#include PIO_HEADER

#endif /* BOARD_NAME */
