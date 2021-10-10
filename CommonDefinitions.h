/***********************************************************************
* @file      CommonDefinitions.h
*
* Common definitions for both the temperature readout application and 
* its test artifacts. Note that those test artifacts are intended to
* encourage Test-Driven Design (TDD).
*
* @brief   
* 
* @note    It is assumed that the C++ Standard Template Library (STL)
*          and the Boost C++ Libraries are available to us. Note that 
*          the intention of relying on Boost is to have available a
*          well-tested implementation of the C++ Networking Technical
*          Specification (future standard). The design of the C++ 
*          Networking TS is based off of the Boost.Asio library.
* 
*          - http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/n4771.pdf
*          - https://en.cppreference.com/w/cpp/experimental
*          - https://en.cppreference.com/w/cpp/experimental/networking
*          - https://github.com/chriskohlhoff/networking-ts-impl
* 
*          Furthermore, I have brought in these three other non-standard 
*          header files as dependencies:
* 
*          [1] My personally developed POSIX threading utilities to aid
*          in development and debug of multi-threaded systems. It is
*          especially useful for debug as different threads can be named,
*          observed and disambiguated via strace, ps, valgrind, gdb, etc.
* 
*          [2] Howard Hinnant's date and time library for C++11/14/17.
*          Note that though this has since been voted into and subsumed
*          into the C++20 standard, I am presuming that the User's 
*          compiler is only up to C++17, per Markem-Imaje dev envronment.
* 
*          - https://github.com/HowardHinnant/date
*          - https://howardhinnant.github.io/date/date.html 
* 
*          [3] Professor M.E. O'Neill's C++11 random number utilities.
*          Lacking in the C++ Standard is that namespace encapsulation 
*          (randutils::) designed by Prof. O'Neill that ties all the 
*          standard C++11 random-number entities and their disparate
*          pieces together, in order to make C++11 random number 
*          generators straightforward to use without sacrificing any of 
*          their power or flexibility.
* 
*         - https://www.pcg-random.org/posts/ease-of-use-without-loss-of-power.html
*
* @warning  
*
* @author  Nuertey Odzeyem
* 
* @date    October 6, 2021
***********************************************************************/
#pragma once

// Boost C++ Libraries:
#include <boost/asio/buffer.hpp>

// The C++ Standard Template Libraries (STL):
#include <type_traits>
#include <algorithm>
#include <functional>                                                                                                    
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <utility>
#include <string>
#include <memory>
#include <chrono>
#include <array>
#include <string_view>

// Non-Standard Headers:
#include "Threading.h"
#include "randutils.hpp"
#include "Date.h"

namespace asio = boost::asio;

using SystemClock_t = std::chrono::system_clock;
using Seconds_t     = std::chrono::seconds;

using boost::asio::ip::tcp;

// Application 'and' sensor nodes are all being tested on my laptop, i.e.
// localhost.
static constexpr std::string_view SENSOR_NODE_STATIC_IP = "127.0.0.1";

static constexpr uint32_t MAXIMUM_TCP_DATA_LENGTH = 87380;

// Customer Requirement:
//
// "1. The readout shall be as close to real time as possible but 
// shall not change faster than once per second."
static constexpr uint8_t MINIMUM_DISPLAY_INTERVAL_SECONDS = 1;

// Customer Requirement:
//
// "3. In case of intermittent communications, temperature readings older
// than 10 minutes shall be considered stale and excluded from the 
// displayed temperature."
static constexpr uint8_t STALE_READING_DURATION_MINUTES = 10;

namespace Utility 
{     
    // Global Random Number Generator (RNG).
    static thread_local randutils::mt19937_rng    gs_theRNG;

    struct RandLibStringGenerator
    {
        RandLibStringGenerator()
            : VALID_CHARACTERS(std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"))
        {
        }

        char operator()() const
        {
            auto index = gs_theRNG.uniform(static_cast<long unsigned int>(0), VALID_CHARACTERS.length()-1);
            return VALID_CHARACTERS[index];
        }

    private:
        const std::string                      VALID_CHARACTERS;
    };

    template <ssize_t N = -1000000, size_t M = 1000000>
    struct NumberGenerator
    {
        NumberGenerator()
        {
        }

        auto operator()() const
        {
            auto number = gs_theRNG.uniform(static_cast<int>(N), static_cast<int>(M));
            return number;
        }

    private:
    };
}
