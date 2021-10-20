#include "SessionManager.h"

void terminator(int signalNumber);

int main([[maybe_unused]]int argc, [[maybe_unused]]char* argv[])
{
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
    
    return 0;
}

void terminator(int signalNumber)
{
    if ((SIGTERM == signalNumber) || (SIGINT == signalNumber) || (SIGQUIT == signalNumber))
    {
        spdlog::warn(
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
