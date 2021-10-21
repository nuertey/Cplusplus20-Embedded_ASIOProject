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

// For POSIX signal handling (i.e. signal requests to terminate 
// application execution):  
#include <signal.h>

// spdlog headers:
#include "spdlog/fmt/bundled/ostream.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/fmt.h"
#include "spdlog/sinks/stdout_color_sinks.h"
//#include "spdlog/async.h" //support for async logging.

// fmt library headers:
#include "fmt/format.h"

// ASIO C++ Networking Library:
#include <asio.hpp>

// The C++ Standard Template Libraries (STL):
#include <ios>
#include <chrono>
#include <string>
#include <memory>
#include <cassert>
#include <utility>
#include <iomanip>
#include <ostream>
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

// Customer Requirement:
//
// "Also assume that the number of nodes is known at compile time, ..."
//
// Assume some default number of sensor nodes then. Feel free though to 
// change the number of sensor nodes when instantiating the template
// for different unit test scenarios.
static constexpr uint8_t NUMBER_OF_SENSOR_NODES = 4;

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

// Handy so we do NOT need to depend upon Boost: RAII class for restoring
// the state of std::cout after manipulating it.
// The big advantage to this technique is if you have multiple return
// paths from a function that sets flags on an iostream. Whichever return
// path is chosen, the destructor will always be called and the flags will
// always be reset. There is no chance of forgetting to restore the flags
// when the function returns.
struct IosFlagSaver
{
    // Leverage RAII design pattern:
    explicit IosFlagSaver(std::ostream& ostr)
        : m_stream(ostr)
        , m_flags(ostr.flags())
    {
    }
    ~IosFlagSaver()
    {
        m_stream.flags(m_flags);
    }

    IosFlagSaver(const IosFlagSaver &rhs) = delete;
    IosFlagSaver& operator= (const IosFlagSaver& rhs) = delete;

private:
    std::ostream&         m_stream;
    std::ios::fmtflags    m_flags;
};

// Metaprogramming types to distinguish the logging category:
struct DebugLog_t {};
struct TraceLog_t {};
struct InfoLog_t {};
struct ErrorLog_t {};
struct WarnLog_t {};
struct CriticalLog_t {};

namespace Utility 
{  
    template <typename T, typename U>
    struct TrueTypesEquivalent : std::is_same<typename std::decay<T>::type, U>::type
    {};
    
    template <typename T>
    constexpr auto TypeName()
    {
        std::string_view name;
        std::string_view prefix;
        std::string_view suffix;
        
    #ifdef __clang__
        name = __PRETTY_FUNCTION__;
        prefix = "auto type_name() [T = ";
        suffix = "]";
    #elif defined(__GNUC__)
        name = __PRETTY_FUNCTION__;
        prefix = "constexpr auto Utility::TypeName() [with T = ";
        suffix = "]";
    #elif defined(_MSC_VER)
        name = __FUNCSIG__;
        prefix = "auto __cdecl type_name<";
        suffix = ">(void)";
    #endif

        name.remove_prefix(prefix.size());
        name.remove_suffix(suffix.size());

        return name;
    }
    
    // Log 'normal' synchronous program flow output to the console but 
    // in a truly thread-safe non-interleaved character way by leveraging
    // spdlog multi-threaded console logger:
    static std::shared_ptr<spdlog::logger>       g_ConsoleLogger;

    inline void InitializeLogger()
    {
        if (!g_ConsoleLogger)
        {
            // Default thread pool settings can be modified *before* 
            // creating the logger(s):
            //
            // Queue with 8k items and 10 backing threads (default is 1).
            //spdlog::init_thread_pool(8192, 10); 

            // Multi-threaded console logger (with color support)
            g_ConsoleLogger = spdlog::stdout_color_mt("console");
            
            spdlog::set_default_logger(g_ConsoleLogger);

            spdlog::set_level(spdlog::level::trace); // Set global log level

            // Customize msg format for all messages
            spdlog::set_pattern("%H:%M:%S %z - %^%l%$ - %^[thread %t]%$ -> %v");
        }
    }
    
    // Essentially, we have turned the synchronous logger into a 
    // singleton; as befits its supposed-to-be usage pattern.
    inline static std::shared_ptr<spdlog::logger>& GetSynchronousLogger()
    {
        // This is a safety check to prevent unforeseen segmentation faults.
        if (!g_ConsoleLogger)
        {
            InitializeLogger();
        }
        
        return g_ConsoleLogger;
    }

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
    
    using SignalHandlerPtr_t = std::add_pointer_t<void(int)>;

    const auto SetupTerminatorSignals = [](SignalHandlerPtr_t signalHandler,
                                           auto ...signalArgs)
    {
        // As is obvious from the above, signalArgs is a variadic template 
        // that specifies the set of 'program-terminating' signals. Per 
        // the sigaction() system call specifications, these signals can
        // comprise any valid set of POSIX signals with the exception of
        // SIGKILL and SIGSTOP. Ensure then to explicitly and forcibly
        // enforce that requirement:
               
        // Leverage 'Fold Expressions in C++17' to resolve the parameter
        // pack expansion.
        (
            assert(((void)"Signal argument CANNOT be SIGKILL or SIGSTOP", 
                   ((signalArgs != SIGKILL) && (signalArgs != SIGSTOP))))
                ,
                ...
        );
        
        // Leverage an RAII-like Design Pattern to setup the sigaction()
        // system call so we catch application 'terminator' signals:
        //
        // int sigaction(int signum, const struct sigaction *restrict act,
        //                                 struct sigaction *restrict oldact);
        
        // 
        struct sigaction action;
        memset(&action, 0, sizeof(struct sigaction));
              
        // Type-check the template type of the signal handler argument
        // at compile-time:
        static_assert((std::is_same_v<SignalHandlerPtr_t, 
                                      decltype(action.sa_handler)>),
                       "Hey! Signal handler function/callback/lambda/class \
                       method MUST satisfy the following type:\n\t\
                       void (*)(int)");

        // As the above static_assert is now confirmed to be correct,  
        // we can now safely assign the signal handler:
        action.sa_handler = signalHandler;
        
        // Leverage 'Fold Expressions in C++17' to resolve the parameter
        // pack expansion.
        (
            sigaction(signalArgs, &action, NULL)
            ,
            ...
        );
    };
    
    // Log colored output to the console but in a truly thread-safe and 
    // non-interleaved character way:
    const auto NonInterspersedLog = []<typename T>(const std::string_view& logMessage,
                                                   auto ...args)
    {
        static_assert((std::is_same_v<T, DebugLog_t>
                    || std::is_same_v<T, TraceLog_t>
                    || std::is_same_v<T, InfoLog_t>
                    || std::is_same_v<T, ErrorLog_t>
                    || std::is_same_v<T, WarnLog_t>
                    || std::is_same_v<T, CriticalLog_t>),
        "Hey! Logging category MUST be one of the following:\n\tDebugLog_t \
                    \n\tTraceLog_t \n\tInfoLog_t \n\tErrorLog_t \
                    \n\tWarnLog_t \n\tFatalLog_t");
        
        char myThreadName[Utility::RECOMMENDED_BUFFER_SIZE];
        Utility::GetThreadName(myThreadName, sizeof(myThreadName));
                   
        std::ostringstream oss;            
        oss << "[" << myThreadName << "] " 
            << "{" << Utility::TypeName<T>() << "}: " 
            << "\"" << logMessage << "\" :-> ";
        
        ((oss << ' ' << args), ...);
            
        std::string logString = oss.str();
                    
        if constexpr (std::is_same_v<T, DebugLog_t> || std::is_same_v<T, TraceLog_t>
                   || std::is_same_v<T, InfoLog_t>)
        {
            std::cout << logString << "\n";
        }
        else if constexpr (std::is_same_v<T, ErrorLog_t> || std::is_same_v<T, WarnLog_t>
                        || std::is_same_v<T, CriticalLog_t>)
        {
            std::cerr << logString << "\n";
        }
    };
}
