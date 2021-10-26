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

// Normally, libunwind supports both local and remote unwinding. However,
// if you tell libunwind that your program only needs local unwinding,
// then a special implementation can be selected which may run much faster
// than the generic implementation which supports both kinds of unwinding.
// To select this optimized version, simply define the macro UNW_LOCAL_ONLY
// before including the headerfile <libunwind.h>. It is perfectly OK for
// a single program to employ both local-only and generic unwinding. 
// That is, whether or not UNW_LOCAL_ONLY is defined is a choice that 
// each source-file (compilation-unit) can make on its own. Independent
// of the setting(s) of UNW_LOCAL_ONLY, you'll always link the same
// library into the program (normally -lunwind). Furthermore, the portion
// of libunwind that manages unwind-info for dynamically generated code
// is not affected by the setting of UNW_LOCAL_ONLY. 
#define UNW_LOCAL_ONLY

// For POSIX signal handling (i.e. signal requests to terminate 
// application execution):  
#include <signal.h>

// For stack frame unwinding/backtracing:
#include <execinfo.h>
#include <cxxabi.h>
#include <libunwind.h>
#include <cstdio>
#include <cstdlib>

// spdlog headers:
#include "spdlog/fmt/bundled/ostream.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/fmt.h"
#include "spdlog/sinks/stdout_color_sinks.h"

//#if __has_include(<format>)
//    #include <format>
//#else
    // fmt library headers:
    #include "fmt/format.h"
//#endif

// TBD Nuertey Odzeyem; the below is how you would use the above in a 
// portable manner once support for std::format is added to the compiler.
#ifdef __cpp_lib_format
    // Code with std::format
#else
    // Code without std::format, or just #error if you only
    // want to support compilers and standard libraries with std::format
#endif

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

namespace Color
{
    enum Code
    {
        // Formatting codes:
        FT_BOLD           =  1,
        FT_DARK           =  2,
        FT_UNDERLINE      =  4,
        FT_BLINK          =  5,
        
        // Foreground colors:
        FG_BLACK          = 30,
        FG_RED            = 31,
        FG_GREEN          = 32,
        FG_YELLOW         = 33,
        FG_BLUE           = 34,
        FG_MAGENTA        = 35,
        FG_CYAN           = 36,
        FG_LIGHT_GRAY     = 37,
        FG_DEFAULT        = 39,
        
        // Background colors:
        BG_RED            = 41,
        BG_GREEN          = 42,
        BG_BLUE           = 44,
        BG_DEFAULT        = 49,
        
        // Foreground colors:
        FG_DARK_GRAY      = 90,
        FG_LIGHT_RED      = 91,
        FG_LIGHT_GREEN    = 92,
        FG_LIGHT_YELLOW   = 93,
        FG_LIGHT_BLUE     = 94,
        FG_LIGHT_MAGENTA  = 95,
        FG_LIGHT_CYAN     = 96,
        FG_WHITE          = 97
    };
    inline std::ostream& operator<<(std::ostream& os, Code code)
    {
        return os << "\033[" << static_cast<int>(code) << "m";
    }
}

// Metaprogramming types to distinguish the logging category:
struct DebugLog_t {};
struct TraceLog_t {};
struct InfoLog_t {};
struct ErrorLog_t {};
struct WarnLog_t {};
struct CriticalLog_t {};

namespace Utility 
{  
    // This utility is for general type comparison cases where the types
    // to be compared's cv qualifiers are not guaranteed. For specific 
    // cases, use std::is_same_v<> directly as in ::NonInterspersedLog<>()
    // templatized lambda far below.
    template <typename T, typename U>
    struct TrueTypesEquivalent : std::is_same<typename std::decay_t<T>, U>::type
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
    
    // TBD Nuertey Odzeyem; this utility could be enhanced to take in an
    // optional FD argument and write the backtrace directly to a file with:
    //
    // backtrace_symbols_fd() takes the same buffer and size arguments as
    // backtrace_symbols(), but instead of returning an array of strings 
    // to the caller, it writes the strings, one per line, to the file
    // descriptor fd.
    const auto CreateBacktrace = []()
    {
        // Since the second argument of backtrace() specifies the maximum
        // number of addresses that can be stored in the buffer specified
        // by the first (man 3 backtrace), a value of 1024 is unnecessarily
        // generous here. A value of 20 should do fine, and printing a
        // warning for possible truncation when bt_size == 20 is good
        // practice.
        const size_t MAX_DEPTH = 100;
        size_t stack_depth;
        void *stack_addrs[MAX_DEPTH];
        char **stack_strings;
        std::ostringstream oss; 

        stack_depth = backtrace(stack_addrs, MAX_DEPTH);
        stack_strings = backtrace_symbols(stack_addrs, stack_depth);

        oss << "Backtrace Call Stack Using GLIBC Utilities:" << std::endl;

        // Skip the first stack frame as it simply points here.
        for (size_t i = 1; i < stack_depth; i++)
        {
            // TBD Nuertey Odzeyem; A guess for now. Enhance later due
            // to in-depth research as template names will be much wider.
            size_t sz = 256; 
            char *function = (char*)malloc(sz);
            char *begin = 0, *end = 0;

            // Find the parentheses and address offset surrounding the
            // mangled name.
            for (char *j = stack_strings[i]; *j; ++j)
            {
                if (*j == '(')
                {
                    begin = j;
                }
                else if (*j == '+')
                {
                    end = j;
                }
            }

            if (begin && end)
            {
                *begin++ = 0;
                std::string full_func_name(end);
                *end = 0; // Found our mangled name, now in [begin, end)

                // The cross-vendor C++ Application Binary Interface. A 
                // namespace alias to __cxxabiv1. GCC subscribes to a 
                // relatively-new cross-vendor ABI for C++, sometimes 
                // called the IA64 ABI because it happens to be the native
                // ABI for that platform. It is summarized at:
                //
                // http://www.codesourcery.com/cxx-abi/ 
                //
                // -----------------------------------------------------
                // New ABI-mandated entry point in the C++ runtime library
                // for demangling. 
                //
                // Function Documentation:
                //
                // char* abi::__cxa_demangle(const char * mangled_name,
                //                           char *       output_buffer,
                //                           size_t *     length,
                //                           int *        status     
                //                          )   
                // Parameters:
                //  mangled_name    A NUL-terminated character string
                //                  containing the name to be demangled.
                //  output_buffer   A region of memory, allocated with
                //                  malloc, of *length bytes, into which 
                //                  the demangled name is stored. If 
                //                  output_buffer is not long enough, it
                //                  is expanded using realloc. output_buffer
                //                  may instead be NULL; in that case, 
                //                  the demangled name is placed in a 
                //                  region of memory allocated with malloc.
                //  length          If length is non-NULL, the length of
                //                  the buffer containing the demangled
                //                  name is placed in *length.
                //  status          *status is set to one of the 
                //                  following values:
                // 
                //       0: The demangling operation succeeded.
                //      -1: A memory allocation failiure occurred.
                //      -2: mangled_name is not a valid name under the C++ ABI mangling rules.
                //      -3: One of the arguments is invalid.
                // 
                // Returns:
                //  A pointer to the start of the NUL-terminated demangled
                //  name, or NULL if the demangling fails. The caller is
                //  responsible for deallocating this memory using free.
                int status;
                char *ret = abi::__cxa_demangle(begin, function, &sz, &status);

                if (ret)
                {
                    // Return value may be a realloc() of the input.
                    function = ret;
                }
                else
                {
                    // Demangling failed, just pretend it's a C function with no args.
                    std::strncpy(function, begin, sz);
                    std::strncat(function, "()", sz);
                    function[sz-1] = 0;
                }

                oss << "    " << stack_strings[i] << ":" << function 
                    << " {" << full_func_name << "}" << std::endl;
            }
            else
            {
                // Did not find the mangled name, just print the whole line.
                oss << "    " << stack_strings[i] << std::endl;
            }
            free(function);
        }

        free(stack_strings); // malloc()ed by backtrace_symbols
        return oss.str();
    };

    // libunwind is the most modern, widespread and portable solution to
    // obtaining the backtrace. It is also more flexible than glibc's
    // backtrace and backtrace_symbols, as it is able to provide extra
    // information such as values of CPU registers at each stack frame.
    const auto CreateLibunwindBacktrace = []()
    {
        std::ostringstream oss; 
        oss << "Backtrace Call Stack Using LIBUNWIND Utilities:" << std::endl;
        
        unw_cursor_t cursor;
        unw_context_t context;

        // Initialize cursor to current frame for local unwinding.
        unw_getcontext(&context);
        unw_init_local(&cursor, &context);

        // Unwind frames one by one, going up the frame stack.
        while (unw_step(&cursor) > 0)
        {
            unw_word_t offset, pc;
            unw_get_reg(&cursor, UNW_REG_IP, &pc);
            if (pc == 0)
            {
                break;
            }
            oss << "0x" << std::setfill('0') << std::setw(12) << std::hex
                << std::uppercase << static_cast<unsigned long int>(pc) << ":";

            char sym[256];
            if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0)
            {
                // output_buffer may instead be NULL; in that case, the
                // demangled name is placed in a region of memory 
                // allocated with malloc.
                char* nameptr = sym;
                int status;
                char* demangled = abi::__cxa_demangle(sym, nullptr, nullptr, &status);
                if (status == 0)
                {
                    nameptr = demangled;
                }
                oss << " (" << nameptr << "+0x" << std::setfill('0')
                    << std::setw(2) << std::hex << std::uppercase 
                    << static_cast<unsigned long int>(offset) << ")\n";

                std::free(demangled);
            }
            else
            {
                oss << " -- error: unable to obtain symbol name for this frame\n";
            }
        }
        
        return oss.str();
    };
    
    using CrashHandlerPtr_t = std::add_pointer_t<void(int, siginfo_t *, void *)>;

    const auto InstallCrashHandler = [](CrashHandlerPtr_t crashHandler,
                                        auto ...signalArgs)
    {
        (assert(((void)"Signal argument CANNOT be SIGKILL or SIGSTOP", (signalArgs != SIGKILL) && (signalArgs != SIGSTOP))), ...);
        
        static constexpr size_t NUMBER_OF_SIGNALS = sizeof...(signalArgs);
        
        struct sigaction action;
        memset(&action, 0, sizeof(struct sigaction));
              
        // Type-check the template type of the signal handler argument
        // at compile-time:
        static_assert((std::is_same_v<CrashHandlerPtr_t, 
                                      decltype(action.sa_sigaction)>),
                       "Hey! Signal handler function/callback/lambda/class \
                       method MUST satisfy the following type:\n\t\
                       void (*)(int, siginfo_t *, void *)");

        // As the above static_assert is now confirmed to be correct,  
        // we can now safely assign the signal handler. Note that on some
        // system architectures, a union is involved; therefore do not 
        // assign to both sa_handler and sa_sigaction.
        sigemptyset(&action.sa_mask);
        action.sa_sigaction = crashHandler;
        action.sa_flags = SA_SIGINFO; // 3rd parameter of handler would 
                                      // be siginfo_t additional data

        if constexpr (0 == NUMBER_OF_SIGNALS)
        {
            // Monitor default core dump signals
            sigaction(SIGBUS,  &action, NULL); // Dump core from bus error (bad memory access) signal.
            sigaction(SIGABRT, &action, NULL); // Dump core from abort signal (cause abnormal process termination).
            sigaction(SIGFPE,  &action, NULL); // Dump core on floating-point exception signal.
            sigaction(SIGSEGV, &action, NULL); // Dump core on invalid memory reference signal (segmentation fault).
            sigaction(SIGILL,  &action, NULL); // Dump core on illegal instruction signal.
            sigaction(SIGQUIT, &action, NULL); // Dump core from keyboard signal (user presses 'ctrl-\').
            sigaction(SIGSYS,  &action, NULL); // Dump core from bad system call (SVr4).
        }
        else
        {
            // Allow the user the choice of specifying which of the core
            // dump signals we want to monitor for.
            (sigaction(signalArgs, &action, NULL), ...);
        }
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
        (assert(((void)"Signal argument CANNOT be SIGKILL or SIGSTOP", (signalArgs != SIGKILL) && (signalArgs != SIGSTOP))), ...);
        
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
        (sigaction(signalArgs, &action, NULL), ...);
    };

    const auto BlockTerminatorSignals = [](auto ...signalArgs)
    {
        // As is obvious from the above, signalArgs is a variadic template 
        // that specifies the set of 'program-terminating' signals. Per 
        // the sigaction() system call specifications, these signals can
        // comprise any valid set of POSIX signals with the exception of
        // SIGKILL and SIGSTOP. Ensure then to explicitly and forcibly
        // enforce that requirement:               
        (assert(((void)"Signal argument CANNOT be SIGKILL or SIGSTOP", (signalArgs != SIGKILL) && (signalArgs != SIGSTOP))), ...);
        
        sigset_t mask;
        sigemptyset(&mask); 
                    
        // Leverage 'Fold Expressions in C++17' to resolve the parameter
        // pack expansion.
        (sigaddset(&mask, signalArgs), ...);
        
        pthread_sigmask(SIG_BLOCK, &mask, NULL);
    };
    
    // Log colored output to the console but in a truly thread-safe and 
    // non-interleaved character way:
    
    // Due to a more involved syntax when invoking template lambdas, 
    // rather prefer template function to a C++20 template lambda.
    template <typename T, typename... Args>
    inline void NonInterspersedLog(const std::string_view& logMessage,
                                   const Args&... args)
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
                    
        if constexpr (std::is_same_v<T, DebugLog_t>)
        {
            std::ostringstream oss;            
            oss << Color::FG_LIGHT_BLUE << "[" << myThreadName << "] " 
                << Color::FG_LIGHT_CYAN << "{" << Utility::TypeName<T>() << "}: " 
                << Color::FG_DEFAULT << "\"" << logMessage << "\"";
            
            ((oss << ' ' << args), ...);
                
            std::string logString = oss.str();
        
            std::cout << logString << "\n";
        }
        else if constexpr (std::is_same_v<T, TraceLog_t>)
        {
            std::ostringstream oss;            
            oss << Color::FG_LIGHT_BLUE << "[" << myThreadName << "] " 
                << Color::FG_MAGENTA << "{" << Utility::TypeName<T>() << "}: " 
                << Color::FG_DEFAULT << "\"" << logMessage << "\"";
            
            ((oss << ' ' << args), ...);
                
            std::string logString = oss.str();
        
            std::cout << logString << "\n";
        }
        else if constexpr (std::is_same_v<T, InfoLog_t>)
        {
            std::ostringstream oss;            
            oss << Color::FG_LIGHT_BLUE << "[" << myThreadName << "] " 
                << Color::FG_GREEN << "{" << Utility::TypeName<T>() << "}: " 
                << Color::FG_DEFAULT << "\"" << logMessage << "\"";
            
            ((oss << ' ' << args), ...);
                
            std::string logString = oss.str();
        
            std::cout << logString << "\n";
        }
        else if constexpr (std::is_same_v<T, ErrorLog_t>)
        {
            std::ostringstream oss;            
            oss << Color::FG_LIGHT_BLUE << "[" << myThreadName << "] " 
                << Color::FG_LIGHT_RED << "{" << Utility::TypeName<T>() << "}: " 
                << Color::FG_DEFAULT << "\"" << logMessage << "\"";
            
            ((oss << ' ' << args), ...);
                
            std::string logString = oss.str();
        
            std::cerr << logString << "\n";
        }
        else if constexpr (std::is_same_v<T, WarnLog_t>)
        {
            std::ostringstream oss;            
            oss << Color::FG_LIGHT_BLUE << "[" << myThreadName << "] " 
                << Color::FG_YELLOW << Color::FT_BOLD << "{" << Utility::TypeName<T>() << "}: " 
                << Color::FG_DEFAULT << "\"" << logMessage << "\"";
            
            ((oss << ' ' << args), ...);
                
            std::string logString = oss.str();
        
            std::cerr << logString << "\n";
        }
        else if constexpr (std::is_same_v<T, CriticalLog_t>)
        {
            std::ostringstream oss;            
            oss << Color::FG_LIGHT_BLUE << "[" << myThreadName << "] " 
                << Color::FG_RED << Color::FT_BOLD << "{" << Utility::TypeName<T>() << "}: " 
                << Color::FG_DEFAULT << "\"" << logMessage << "\"";
            
            ((oss << ' ' << args), ...);
                
            std::string logString = oss.str();
        
            std::cerr << logString << "\n";
        }
    };
}
