/***********************************************************************
* @file      SessionManager.h
*
* SessionManager encapsulating and controlling ALL the socket connections
* to the temperature sensor nodes. It also comprises the main business 
* logic of the operations to be performed when temperature data is
* received on those sockets.
*
* @brief   
* 
* @note     Be aware that I have designed things in such a way that most
*           of the socket operations that occur within this module will 
*           occur asynchronously. 
* 
*           Asynchronicity is the Mother of Speed, Nimbleness and 
*           Responsiveness. 
* 
*           To sate thy righteous Curiosity, the POSIX networking I/O 
*           paradigm that  would be equivalent to ASIO would be epoll().
*
* @warning  
*
* @author  Nuertey Odzeyem
* 
* @date    October 10, 2021
***********************************************************************/
#pragma once

#include <mutex>
#include <array>
#include <thread>
#include <optional>
#include "CommonDefinitions.h"

namespace Common
{
    using ThreadPack_t        = std::array<std::thread, DISPATCHER_THREAD_POOL_SIZE>;
    //using ExecutorWorkGuard_t = asio::executor_work_guard<asio::io_context::executor_type>;
    
    // any_io_executor is a type-erasing executor wrapper. 
    //
    // execution::any_executor class template can be parameterized as 
    // follows:
    //
    // @code execution::any_executor<
    //   execution::context_as_t<execution_context&>,
    //   execution::blocking_t::never_t,
    //   execution::prefer_only<execution::blocking_t::possibly_t>,
    //   execution::prefer_only<execution::outstanding_work_t::tracked_t>,
    //   execution::prefer_only<execution::outstanding_work_t::untracked_t>,
    //   execution::prefer_only<execution::relationship_t::fork_t>,
    //   execution::prefer_only<execution::relationship_t::continuation_t>
    // > @endcode
    using ExecutorWorkGuard_t = asio::any_io_executor;

    extern asio::io_context                     g_DispatcherIOContext;
    extern std::optional<ExecutorWorkGuard_t>   g_DispatcherWork;
    extern ThreadPack_t                         g_DispatcherWorkerThreads;

    void SetupIOContext();
    void RunWorkerThreads();
    void JoinWorkerThreads();
    void DestroyWorkerThreads();

    // By default 'auto' has external linkage therefore restrict it
    // with 'const' to ensure that multiple symbol definition error is 
    // not encountered during linking. 
    const auto DispatcherWorkerThread = []()
    {
        // Safety check:
        Utility::InitializeLogger();
        
        // To aid debugging by means of strace, ps, valgrind, gdb, and
        // variants, name our created threads. 
        std::string namePrefix("WorkerThread_");
        std::string nameSuffix(3, '*');
        Utility::RandLibStringGenerator generator;
        std::generate(nameSuffix.begin(), nameSuffix.end(), generator);
        std::string uniqueName = namePrefix + nameSuffix;

        char myThreadName[Utility::RECOMMENDED_BUFFER_SIZE];
        Utility::SetThreadName(uniqueName.c_str());
        Utility::GetThreadName(myThreadName, sizeof(myThreadName));

        Utility::GetSynchronousLogger()->info("Parent just created a thread. ThreadName = {0}", myThreadName);

        // TBD Nuertey Odzeyem; were I truly compiling on an Embedded
        // system, I would have optimized these C++ exceptions completely
        // out of the source code.
        try
        {
            // Though blocked here, note that it does NOT imply that we 
            // are still being scheduled by the processor or perpetually
            // polling the processor. In fact we would have been taken 
            // out of the kernel scheduling queues completely and rather
            // placed into the kernel waiting queues. Hence the processor
            // is 100% free of us to devote its time to other tasks. 
            // Polling is grossly inefficient and ought to be obsoleted.

            // Only when some event or trigger asynchronously arrives and
            // is subsequently transformed into the synchronous post() 
            // within this worker thread's context, will we be scheduled
            // to run by the processor/kernel scheduler to actually 
            // process that event however we wish.
            g_DispatcherIOContext.run();
        }
        catch (const std::exception& e)
        {
            Utility::GetSynchronousLogger()->error("Caught an exception! {0}", e.what());
        }

        Utility::GetSynchronousLogger()->warn("Exiting Dispatcher Worker Thread {0}", myThreadName);
    };
}

// Metaprogramming types to distinguish the logging category:
struct DebugLog_t {};
struct TraceLog_t {};
struct InfoLog_t {};
struct ErrorLog_t {};
struct WarnLog_t {};
struct CriticalLog_t {};

class SessionManager : public std::enable_shared_from_this<SessionManager>
{
    static constexpr short EPHEMERAL_PORT_NUMBER_BASE_VALUE = 5000;
    
public:
    SessionManager();
    virtual ~SessionManager();

    void Start();
    
    template<typename T, typename... Args>
    void AsyncLog(const std::string& logMessage, Args&&... args);

protected:
    void StartConnect(const uint8_t& sensorNodeNumber);
    void AsyncConnect(const uint8_t& sensorNodeNumber, tcp::resolver::iterator& it);
    void HandleConnect(const std::error_code& error, const uint8_t& sensorNodeNumber,
                       tcp::resolver::iterator& endpointIter);
    void ReceiveTemperatureData(const uint8_t& sensorNodeNumber);
    void DisplayTemperatureData();

private:
    uint8_t                     m_NumberOfConnectedSockets;
    std::mutex                  m_TheDisplayMutex;
    SystemClock_t::time_point   m_LastReadoutTime;

public:    
    // Log exceptions output to the console but in a truly thread-safe 
    // non-interleaved character way by leveraging spdlog async logger:
    //static std::shared_ptr<spdlog::logger>   m_pAsyncLogger;
};

template<typename T, typename... Args>
void SessionManager::AsyncLog(const std::string& logMessage, Args&&... args)
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
    
    std::ostringstream oss;            
    oss << logMessage;
    
    // Leverage fold expressions in C++17 to resolve the parameter pack
    // expansion:
    ((oss << ' ' << args), ...);
        
    std::string logString = oss.str();
                
    if constexpr (std::is_same_v<T, DebugLog_t>)
    {
        //Utility::GetSynchronousLogger()->debug("{}", logString);
        std::cout << logString << "\n";
    }
    else if constexpr (std::is_same_v<T, TraceLog_t>)
    {
        //Utility::GetSynchronousLogger()->trace("{}", logString);
        std::cout << logString << "\n";
    }
    else if constexpr (std::is_same_v<T, InfoLog_t>)
    {
        //Utility::GetSynchronousLogger()->info("{}", logString);
        std::cout << logString << "\n";
    }
    else if constexpr (std::is_same_v<T, ErrorLog_t>)
    {
        //Utility::GetSynchronousLogger()->error("{}", logString);
        std::cerr << logString << "\n";
    }
    else if constexpr (std::is_same_v<T, WarnLog_t>)
    {
        //Utility::GetSynchronousLogger()->warn("{}", logString);
        std::cerr << logString << "\n";
    }
    else if constexpr (std::is_same_v<T, CriticalLog_t>)
    {
        //Utility::GetSynchronousLogger()->critical("{}", logString);
        std::cerr << logString << "\n";
    }
    else
    {
        std::cerr << "WARN: Type T is UNEXPECTED!" << "\n";
        std::cerr << "\t" << Utility::TypeName<T>() << "\n";
        std::cerr << "\t\"" << logString << "\"\n";
    }
}
