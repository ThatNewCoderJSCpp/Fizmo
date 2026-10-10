#ifndef TIME_INCLUDES_HPP
#define TIME_INCLUDES_HPP

#include "../Source/Time/frame_clock.hpp"
#include "../Source/Time/stopwatch.hpp"
#include "../Source/Time/timer.hpp"

#if defined(OS_LINUX) && defined(FIZMO_TIME_USE_TSC)
#include "../Source/Time/tsc.hpp"
#endif

#endif //TIME_INCLUDES_HPP