#ifndef FIZMO_BASIC_DEFINE_INCLUDES_HPP
#define FIZMO_BASIC_DEFINE_INCLUDES_HPP

#include "../Source/Basic/basic_includes.hpp"
#include "../Source/Symbolic Math/Common/points.hpp"
#include "../Source/Multiprecision/include.hpp"
#include "../Source/Random/random_std_int.hpp"
#include "../Source/Random/random_mp.hpp"
#include "../Source/Random/random_text.hpp"
#include "../Source/Random/random_generator.hpp"

#if defined(FIZMO_BASIC) || defined(FIZMO_BASICS)
    #include "std_overloads.hpp"
#endif

#ifdef FIZMO_CONSTANTS
    #include "../Source/Basic/constants.hpp"
#endif 

#if defined(FIZMO_MISC) || defined(FIZMO_MISCELLANEOUS)
    #include "../Source/Basic/endianess.hpp"
#endif

#endif // FIZMO_BASIC_DEFINE_INCLUDES_HPP