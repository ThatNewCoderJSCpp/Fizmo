#ifndef ALL_FIZMO_INCLUDES_FILE_HPP
#define ALL_FIZMO_INCLUDES_FILE_HPP
    #if __cplusplus < 201703L
        #error "Fizmo is only available in C++17 (201703L) or newer"
    #endif

    #if !(defined(__linux__) || defined(_WIN32) || defined(_WIN64))
        #error "Fizmo is only available on Windows and Linux"
    #endif

    #ifdef ALL_FIZMO
        #ifndef FIZMO
            #define FIZMO
        #endif
        #ifndef FIZMO_BASICS
            #define FIZMO_BASICS
        #endif
        #ifndef FIZMO_GRAPHICS
            #define FIZMO_GRAPHICS
        #endif
        #ifndef FIZMO_LOW_LEVEL
            #define FIZMO_LOW_LEVEL
        #endif
        #ifndef FIZMO_VECTORS
            #define FIZMO_VECTORS
        #endif
        #ifndef FIZMO_PHYSICS
            #define FIZMO_PHYSICS
        #endif
        #ifndef FIZMO_MATH
            #define FIZMO_MATH
        #endif
        #ifndef FIZMO_TIME
            #define FIZMO_TIME
        #endif
        #ifndef FIZMO_CONVERTERS
            #define FIZMO_CONVERTERS
        #endif
        #ifndef FIZMO_TUPLE
            #define FIZMO_TUPLE
        #endif
        #ifndef FIZMO_STRING
            #define FIZMO_STRING
        #endif
        #ifndef FIZMO_ARRAY
            #define FIZMO_ARRAY
        #endif
        #ifndef FIZMO_STATISTICS
            #define FIZMO_STATISTICS
        #endif
        #ifndef FIZMO_MISC
            #define FIZMO_MISC
        #endif
        #ifndef FIZMO_CHEMISTRY
            #define FIZMO_CHEMISTRY
        #endif
        #ifndef FIZMO_MATRICES
            #define FIZMO_MATRICES
        #endif

        #ifndef FIZMO_CONSTANTS
            #define FIZMO_CONSTANTS
        #endif
    #endif // ALL_FIZMO

    #ifdef FIZMO
        #include "../basics.hpp" // More #if checks inside file
        #include "../util.hpp"

        #ifdef FIZMO_GRAPHICS
            #include "../custom_graphics.hpp"
            #include "../images.hpp"
        #endif 

        #ifdef FIZMO_VECTORS
            #include "../basic_vectors.hpp"
        #endif 

        #ifdef FIZMO_PHYSICS
            #include "../physics.hpp"
        #endif 

        #ifdef FIZMO_MATH
            #include "../misc_math.hpp"
            #include "../std_overloads.hpp"

            #include "../complex.hpp"
            
            #include "../number_theory.hpp"
            #include "../calculus.hpp"
        #endif

        #if defined(FIZMO_TIME) || defined(FIZMO_CHRONO)
            #include "../time.hpp"
        #endif 

        #if defined(FIZMO_CONVERTER) || defined(FIZMO_CONVERTERS)
            #include "../converters.hpp"
        #endif 

        #if defined(FIZMO_STRING) || defined(FIZMO_STR)
            #include "../string.hpp"
        #endif

        #if defined(FIZMO_ARRAY) || defined(FIZMO_ARR)
            #include "../arrays.hpp"
        #endif

        #if defined(FIZMO_CHEMISTRY) || defined(FIZMO_CHEM)
            #include "../chemistry.hpp"
        #endif

        #ifdef FIZMO_MATRICES
            #include "../matrices.hpp"
        #endif
    #endif // FIZMO
#endif //ALL_FIZMO_INCLUDES_FILE_HPP