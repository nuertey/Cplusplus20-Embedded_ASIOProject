#include "SessionManager.h"

void terminator(int signalNumber);

int main([[maybe_unused]]int argc, [[maybe_unused]]char* argv[])
{
    Utility::NonInterspersedLog<TraceLog_t>("Beginning C++20 Design Exercise Program...");
    
    Utility::InitializeLogger();
    
    // Setup and run the one worker thread and one io_context that we 
    // need to successfully serialize all operations expected from the 
    // potentially several asynchronous socket instances. See the C++
    // Networking Technical Specification standard for the details
    // undergirding io_context and its usage:
    //
    // http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/n4771.pdf
    Common::SetupIOContext();
    Common::RunWorkerThreads();

    // TBD Nuertey Odzeyem; we can further guarantee deterministic
    // nature of signal handling and on which particular thread 
    // 'terminator' signal handler is always chosen to run by blocking
    // those terminator signals within ALL other children thread contexts.
    Utility::SetupTerminatorSignals(terminator, SIGTERM, SIGINT, SIGQUIT);

    // Be aware that if the program is forcibly halted whilst the SessionManager
    // is still constructing and connecting to the sockets, then by design,
    // the program will throw an exception before exiting. Logically, 
    // that exception is: "what():  connect: Interrupted system call"
    
    // Also, always ensure to assign the shared_ptr to a temporary otherwise
    // it will run out of scope and the socket(s) will segfault with:
    //
    // [WARN] Error in reading from TCP socket connection:
    // 127.0.0.1:5000
    // Value := "Code: 125
    //  Category: system
    //  Message: Operation canceled
    auto theSessionManager = std::make_shared<SessionManager>();
    theSessionManager->Start();

    // Block and wait on the worker threads until they have completed
    // processing ALL 'work' (past, present and future) to be scheduled
    // from the potentially many asynchronuous socket instances, and are 
    // ready to exit.
    Common::JoinWorkerThreads();
    
    // Release and close all loggers
    //spdlog::drop_all();
    spdlog::shutdown();
    
    Utility::NonInterspersedLog<TraceLog_t>("Ending C++20 Design Exercise Program...");

    return 0;
}

void terminator(int signalNumber)
{
    if ((SIGTERM == signalNumber) || (SIGINT == signalNumber) || (SIGQUIT == signalNumber))
    {
        // One a single threaded process, when a signal arrives, that 
        // main thread usually completes its currently-executing 
        // instruction, services the signal via the signal handler before
        // returning to resume its former execution path. So in this 
        // scenario then, the signal handler context can be said to be 
        // the same as the single main thread's context.
        //
        // Contrasting the above, on multi-threaded systems however, when
        // a signal arrives, it is--for our purposes--effectively randomly
        // scheduled on any of the competing threads. One cannot then
        // predict on which thread the signal handler would execute. 
        // Most likely, it would be executed on the most idle thread 
        // which in our present case is the main thread (idling on 
        // Common::JoinWorkerThreads()). But that is rather a wish than a
        // prediction.
        //
        // The caveat to all the above is, if the signal is an exception
        // (SIGSEGV, SIGFPE, SIGBUS, SIGILL, …) that signal would 
        // usually be caught by the thread raising that exception.
        //
        // Further note that though signal handlers are written to accept
        // signals destined per process, signal masks rather pertain to 
        // threads. Hence take this into account when forking processes 
        // and/or spawning off threads.
        
        //spdlog::warn(
        //"Signal Received. Closing application orderly, cleanly and gracefully.\n\n");
        Utility::NonInterspersedLog<WarnLog_t>(
        "Signal Received. Closing application orderly, cleanly and gracefully.\n\n");
        
        // This call is designed to be thread-safe so go ahead and invoke
        // it from the asynchronous signal context.
        Common::DestroyWorkerThreads(); 
        
        // Customer Requirement:
        //
        // "... or if the application terminates, the readout shall 
        // display “--.- °C”."
        std::cout << "\t\t--.- °C" << "\n";
    }
}
