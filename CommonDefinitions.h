/***********************************************************************
* @file      CommonDefinitions.h
*
* Common definitions for both the temperature readout application and 
* its test artifacts. Note that those test artifacts are intended to
* encourage Test-Driven Design (TDD).
*
* @brief   
* 
* @note     It is assumed that the C++ Standard Template Library (STL)
*           and the ASIO C++ Networking Library are available to us.
* 
*           Furthermore, I have brought in these two other non-standard 
*           header files as dependencies:
* 
*           [1] My personally developed POSIX threading utilities to aid
*           in development and debug of multi-threaded systems. It is
*           especially useful for debug as different threads can be named,
*           observed and disambiguated via strace, ps, valgrind, gdb, etc.
* 
*           [2] Professor M.E. O'Neill's C++11 random number utilities.
*           Lacking in the C++ Standard is that namespace encapsulation 
*           (randutils::) designed by Prof. O'Neill that ties all the 
*           standard C++11 random-number entities and their disparate
*           pieces together, in order to make C++11 random number 
*           generators straightforward to use without sacrificing any of 
*           their power or flexibility.
* 
*         - https://www.pcg-random.org/posts/ease-of-use-without-loss-of-power.html
*
* @warning  Be aware that I have designed things in such a way that most
*           of the socket operations that occur within this module will 
*           occur asynchronously. 
* 
*           Asynchronicity is the Mother of Speed, Nimbleness and 
*           Responsiveness. 
* 
*           To sate thy righteous Curiosity, the POSIX networking I/O 
*           paradigm that  would be equivalent to ASIO would be epoll().
*
* @author  Nuertey Odzeyem
* 
* @date    October 9, 2021
***********************************************************************/
#pragma once

// ASIO C++ Networking Library:
#include <asio.hpp>

// The C++ Standard Template Libraries (STL):
#include <chrono>
#include <string>
#include <memory>
#include <utility>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <string_view>
#include <system_error>

// Non-Standard Headers:
#include "Threading.h"
#include "randutils.hpp"

using SystemClock_t = std::chrono::system_clock;
using Seconds_t     = std::chrono::seconds;
using Minutes_t     = std::chrono::minutes;

using asio::buffer;
using asio::ip::tcp;

// Application 'and' sensor nodes are all being tested on my laptop, 
// i.e. localhost. 
//
// Note that a string_view is a string-like object that acts as an 
// immutable, non-owning reference to any sequence of char objects. We
// need string_view here because C++17 does NOT support std::string in
// constexpr. C++20 though, does.
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

// One thread and one io_context is all we need to successfully  
// serialize all operations invoked from several asynchronous contexts.
// Implicit in that one thread is an "implicit strand". Going forward,
// should multiple threads be required, enabled and used, then an explicit
// strand (i.e. asio::strand) will need to be declared and used to protect
// and serialize operations to the io_context.
static constexpr std::size_t DISPATCHER_THREAD_POOL_SIZE = 1;

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
