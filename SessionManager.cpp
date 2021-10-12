#include "SessionManager.h"

namespace Common
{
    asio::io_context                     g_DispatcherIOContext;
    std::optional<ExecutorWorkGuard_t>   g_DispatcherWork;
    //std::optional<ExecutorWorkGuard_t> g_DispatcherWork(g_DispatcherIOContext.get_executor());
    ThreadPack_t                         g_DispatcherWorkerThreads;

    void SetupIOContext()
    {
        //g_DispatcherWork.emplace(asio::make_work_guard(g_DispatcherIOContext));
        
        // Stopping the io_context from running out of work
        //
        // Some applications may need to prevent an io_context object's 
        // run() call from returning when there is no more work to do. 
        // For example, the io_context may be being run in a background 
        // thread that is launched prior to the application's asynchronous
        // operations. The run() call may be kept running by creating an
        // executor that tracks work against the io_context:
        g_DispatcherWork.emplace(
                     asio::require(g_DispatcherIOContext.get_executor(),
                     asio::execution::outstanding_work.tracked)
                     );
    }

    void RunWorkerThreads()
    {
        for (std::size_t i = 0; i < DISPATCHER_THREAD_POOL_SIZE; ++i)
        {
            // Create the std::thread that will wait for ALL 'work' 
            // (past, present and future) to be scheduled from the 
            // potentially many asynchronous socket instances.
            g_DispatcherWorkerThreads[i] = std::move(std::thread(DispatcherWorkerThread));
        }
    }

    void JoinWorkerThreads()
    {
        try
        {
            for (auto& thread : g_DispatcherWorkerThreads)
            {
                if (thread.joinable())
                {
                    thread.join();
                }
            }
        }
        catch (const std::exception& e)
        {
            Utility::g_ConsoleLogger->error("Caught an exception! {0}", e.what());
        }
    }

    void DestroyWorkerThreads()
    {
        // Alternatively, if the application requires that all operations
        // and handlers be allowed to finish normally, ensure to store
        // the work-tracking executor in an any_io_executor object, so 
        // that it may be explicitly reset:
        g_DispatcherWork = ExecutorWorkGuard_t(); // Allow run() to exit.
        
        // If the application requires that all operations and handlers 
        // be allowed to finish normally, the work object may be explicitly
        // destroyed:
        
        // Allow io_context.run() to naturally and gracefully exit.
        // Work guard is destroyed, io_context::run is free to return.
        //g_DispatcherWork.reset(); 
        
        // To effect a shutdown, the application will then need to call 
        // the io_context object's stop() member method. This will cause
        // the io_context run() call to return as soon as possible, thus 
        // abandoning unfinished operations and without permitting ready
        // handlers to be dispatched. 

        // IMPORTANT ADDENDUM!:
        //
        // Per observed results in past usages and mocked-up peripherally
        // tested scenarios, I recommend that we never stop the io_context.
        // It is far more prudent to allow the io_context to naturally
        // complete its scheduled tasks before naturally exiting. Failure
        // to adhere to this rule will likely cause the io_context to 
        // exit prematurely, way before the worker(s) are done working.
        // The keyword here is "likely". For we are even 'lucky' to 
        // observe such relatively benign behavior/results such as in my
        // testing scenarios. For, per the ASIO/C++ Networking TS
        // standard, calling g_DispatcherIOContext.stop() or reset() 
        // while there are unfinished run(), run_one(), poll() or 
        // poll_one() pending calls WILL result in undefined behavior. 
        // And "undefined behavior" (UB) is just that--undefined. 
        // Its results are equally likely to be the manifestation of a
        // quantum shift precipitating the onset of a black hole which 
        // lugubriously will swallow the earth whole and make it 
        // disappear :). Simply put, do not invoke undefined behavior
        // nor depend upon any of its side-effects observed at any time.
        //
        g_DispatcherIOContext.stop();
    }
}

// Customer Requirement:
//
// "Each node has a static IP, listens on a port, accepts a connection,
// and then sends the latest temperature reading, in deg C, on one line
// of ascii text."
using TcpData_t = std::array<char, MAXIMUM_TCP_DATA_LENGTH>;

struct SensorNode_t
{
    SensorNode_t()
        : m_Host(SENSOR_NODE_STATIC_IP) // Same test laptop, same LAN, same IP=localhost.
        , m_Port()
        , m_ConnectionSocket(Common::g_DispatcherIOContext)
        , m_TcpData()
        , m_CurrentReadingTime()
    {
    }
        
    virtual ~SensorNode_t()
    {
    }
    
    std::string                m_Host; // TCP host.
    std::string                m_Port; // TCP port number.
    tcp::socket                m_ConnectionSocket;
    TcpData_t                  m_TcpData;
    std::string                m_CurrentTemperatureReading;
    SystemClock_t::time_point  m_CurrentReadingTime;
};

using SensorPack_t = std::array<SensorNode_t, NUMBER_OF_SENSOR_NODES>;

// Also value-initialize all sensor node abstractions.
static SensorPack_t g_TheCustomerSensors{};

SessionManager::SessionManager()
    : m_NumberOfConnectedSockets(0)
    , m_TheDisplayMutex()
    , m_LastReadoutTime()
{
    // Initialize variable values for all sensor node abstractions.
    for (size_t i = 0; i < g_TheCustomerSensors.size(); i++) 
    {
        // Use a different port for each sensor node.
        g_TheCustomerSensors[i].m_Port = std::to_string(EPHEMERAL_PORT_NUMBER_BASE_VALUE + i);
        
        // All operations to occur on ALL the socket connections will
        // occur asynchronously but in the same worker thread context 
        // and on the same ASIO io_context. Asynchronicity will
        // guarantee us the fastest most nimble response. i.e. realtime.
        
        // Note that since tcp::socket is not default constructible nor 
        // assignable, we already explicitly constructed 
        // g_TheCustomerSensors[i].m_ConnectionSocket as required in the
        // SensorNode_t default constructor.
        
        // No temperature reading as yet.
        g_TheCustomerSensors[i].m_TcpData = {}; // Initialize to zeros.
        g_TheCustomerSensors[i].m_CurrentTemperatureReading = "";
        
        // Since no readings exist as yet, default all readings to stale.
        g_TheCustomerSensors[i].m_CurrentReadingTime = SystemClock_t::now()
                - Minutes_t(STALE_READING_DURATION_MINUTES + 1);
    }
    
    // Initial display.
    {
        // Always protect the display abstraction via mutual exclusion.
        std::unique_lock<std::mutex> lock(m_TheDisplayMutex);

        // Customer Requirement:
        //
        // "4. If no temperature readings are available, ... , the readout 
        // shall display “--.- °C”."
        std::cout << "\t\t--.- °C" << "\n";
        
        m_LastReadoutTime = SystemClock_t::now();
    }
}

SessionManager::~SessionManager()
{
}

void SessionManager::Start()
{        
    // Attempt to connect to ALL the temperature sensor nodes.
    for (size_t i = 0; i < g_TheCustomerSensors.size(); i++) 
    {
        StartConnect(i);
    }
}

void SessionManager::StartConnect(const uint8_t& sensorNodeNumber)
{   
    tcp::resolver resolver1(Common::g_DispatcherIOContext);
    tcp::resolver::query query1(tcp::v4(), 
                                g_TheCustomerSensors[sensorNodeNumber].m_Host.c_str(), 
                                g_TheCustomerSensors[sensorNodeNumber].m_Port.c_str());
    tcp::resolver::iterator destination1 = resolver1.resolve(query1);

    if (destination1 != asio::ip::tcp::resolver::iterator()) 
    {
        AsyncConnect(sensorNodeNumber, destination1);
    }   
    else
    {
        Utility::g_ConsoleLogger->error("Could not resolve IP address query :-> \"{0}:{1}\"",
                 g_TheCustomerSensors[sensorNodeNumber].m_Host.c_str(),
                 g_TheCustomerSensors[sensorNodeNumber].m_Port.c_str());
    }
}

void SessionManager::AsyncConnect(const uint8_t& sensorNodeNumber, 
                                  tcp::resolver::iterator& it)
{
    using namespace std::placeholders;

    // Stringently manage our object lifetime even through callbacks, 
    // with the appropriate C++ lambda captures on shared_ptr to self.
    auto self(shared_from_this());
    
    // In truly asynchronous contexts, we must always invoke this
    // method as a safety check before logging. This will ensure that
    // the logger shared_ptr is initialized properly if it does not
    // exist, before attempting to dereference it.
    Utility::InitializeLogger();
    
    if (!g_TheCustomerSensors[sensorNodeNumber].m_ConnectionSocket.is_open())
    {
        tcp::endpoint endpoint1;

        if (it != asio::ip::tcp::resolver::iterator()) 
        {
            endpoint1 = *it;
            Utility::g_ConsoleLogger->debug("Connecting to TCP endpoint :-> {0}",
                 endpoint1);
                      
            g_TheCustomerSensors[sensorNodeNumber].m_ConnectionSocket.async_connect(endpoint1,
                             std::bind(&SessionManager::HandleConnect,
                                       this, _1, sensorNodeNumber, it));
        }
        else
        {
            Utility::g_ConsoleLogger->warn("Giving up on connecting to:\n\t\"{0}:{1}\"\n\tValue := \"{2}\"",
                 g_TheCustomerSensors[sensorNodeNumber].m_Host,
                 g_TheCustomerSensors[sensorNodeNumber].m_Port,
                 std::string("Exhausted resolved endpoints list!"));
          
            Utility::g_ConsoleLogger->warn("Ensure to a priori launch the sensor node test application(s).");
        }
    }   
}

void SessionManager::HandleConnect(const std::error_code& error, 
                                   const uint8_t& sensorNodeNumber,
                                   tcp::resolver::iterator& endpointIter)
{
    // Stringently manage our object lifetime even through callbacks, 
    // with the appropriate C++ lambda captures on shared_ptr to self.
    auto self(shared_from_this());
    
    // In truly asynchronous contexts, we must always invoke this
    // method as a safety check before logging. This will ensure that
    // the logger shared_ptr is initialized properly if it does not
    // exist, before attempting to dereference it.
    Utility::InitializeLogger();
    
    if (!error)
    {
        // The async_connect() function automatically opens the socket 
        // at the start of the asynchronous operation. If for some reason
        // the socket was closed in the interim, then retry the next 
        // available endpoint for the same sensor. 
        if (!g_TheCustomerSensors[sensorNodeNumber].m_ConnectionSocket.is_open())
        {
            Utility::g_ConsoleLogger->error("Failure in connecting to TCP socket:\n\t{0}\n\tValue := \"{1}\"",
                 endpointIter->endpoint(),
                 std::string("Connection somehow timed out."));

            // Try the next available endpoint for the same sensor.
            AsyncConnect(sensorNodeNumber, ++endpointIter);
        }
        else
        {
            // Otherwise we have successfully established a connection.
            Utility::g_ConsoleLogger->trace("Successfully connected to \"{0}\"",
                 endpointIter->endpoint());
                      
            ++m_NumberOfConnectedSockets;
            if (NUMBER_OF_SENSOR_NODES == m_NumberOfConnectedSockets)
            {
                Utility::g_ConsoleLogger->trace("ALL temperature sensor nodes have been successfully connected to.");
            }

            // Proceed to reading temperature readings and exercising the
            // business logic to display to the user per the customer 
            // requirements:
            
            // Attempt to asynchronously read this sensor.
            ReceiveTemperatureData(sensorNodeNumber);
        }
    }
    else
    {
        std::ostringstream oss;

        oss << "Code: " << error.value() << '\n';
        oss << "\t\tCategory: " << error.category().name() << '\n';
        oss << "\t\tMessage: " << error.message() << '\n';

        Utility::g_ConsoleLogger->error("Failure in connecting to TCP socket:\n\t{0}\n\tValue := \"{1}\"",
             endpointIter->endpoint(),
             oss.str());
                  
        // We need to close the socket used in the previous connection
        // attempt before re-attempting to start a new one.
        g_TheCustomerSensors[sensorNodeNumber].m_ConnectionSocket.close();

        // Try the next available endpoint for the same sensor.
        AsyncConnect(sensorNodeNumber, ++endpointIter);
    }
}

void SessionManager::ReceiveTemperatureData(const uint8_t& sensorNodeNumber)
{
    // Stringently manage our object lifetime even through callbacks, 
    // with the appropriate C++ lambda captures on shared_ptr to self.
    auto self(shared_from_this());
    
    // In truly asynchronous contexts, we must always invoke this
    // method as a safety check before logging. This will ensure that
    // the logger shared_ptr is initialized properly if it does not
    // exist, before attempting to dereference it.
    Utility::InitializeLogger();
    
    // Note that though asio::ip::tcp::socket is NOT thread safe,
    // we are guaranteed safe operation as we are receiving asynchronously
    // and sequentially per sensor node socket.
    
    // Use an ad-hoc lambda completion handler for asynchronous operation.
    // & (implicitly capture the used automatic variables by reference).
    g_TheCustomerSensors[sensorNodeNumber].m_ConnectionSocket.async_receive(
         asio::buffer(g_TheCustomerSensors[sensorNodeNumber].m_TcpData),
    [&](const std::error_code& error, std::size_t length)
    {
        // In truly asynchronous contexts, we must always invoke this
        // method as a safety check before logging. This will ensure that
        // the logger shared_ptr is initialized properly if it does not
        // exist, before attempting to dereference it.
        Utility::InitializeLogger();
        
        if (!error)
        {
            // Debug prints...
            //Utility::g_ConsoleLogger->debug("{0}\n",
            //   std::string(g_TheCustomerSensors[sensorNodeNumber].m_TcpData.data(), length));
            
            // This is the sensor temperature reading that we received.
            g_TheCustomerSensors[sensorNodeNumber].m_CurrentTemperatureReading
              = std::string(g_TheCustomerSensors[sensorNodeNumber].m_TcpData.data(),
                            length);
            
            // Note the time at which we received that sensor reading.
            g_TheCustomerSensors[sensorNodeNumber].m_CurrentReadingTime = SystemClock_t::now();

            // Escape the asynchronous context, and schedule/enter the
            // readout display method on the worker thread context so that
            // we can safely lock the display mutex before attempting to
            // display. Without this precaution, we might deadlock.
            asio::post(Common::g_DispatcherIOContext, 
                       std::bind(&SessionManager::DisplayTemperatureData,
                       this));
        }
        else
        {
            std::ostringstream oss;

            oss << "Code: " << error.value() << '\n';
            oss << "\t\tCategory: " << error.category().name() << '\n';
            oss << "\t\tMessage: " << error.message() << '\n';

            Utility::g_ConsoleLogger->error("Failure in reading from TCP socket connection:\n\t\"{0}:{1}\"\n\tValue := \"{2}\"",
                 g_TheCustomerSensors[sensorNodeNumber].m_Host,
                 g_TheCustomerSensors[sensorNodeNumber].m_Port,
                 oss.str());
        }
        
        // Customer Requirement:
        //
        // "3. In case of intermittent communications, temperature readings 
        // older than 10 minutes shall be considered stale and excluded 
        // from the displayed temperature."
        
        // Presumably, the above Customer requirement referencing 
        // "intermittent communications" implies that on failure to 
        // receive on any particular socket, we ought to, regardless,
        // try again. The Customer seems to expect this. Alternative to
        // this design decision would have been moving the next execution
        // statement to after the "boost::asio::post()" statement on line 200. 
        //
        // Here then goes: 
        
        // ... Do not forget to set up asynchronous read handler again.
        ReceiveTemperatureData(sensorNodeNumber);
    });
}

void SessionManager::DisplayTemperatureData()
{
    // Stringently manage our object lifetime even through callbacks, 
    // with the appropriate C++ lambda captures on shared_ptr to self. 
    auto self(shared_from_this());
    
    // Always protect the display abstraction via mutual exclusion.
    std::unique_lock<std::mutex> lock(m_TheDisplayMutex);

    // Customer Requirement:
    //
    // "1. The readout shall be as close to real time as possible but 
    // shall not change faster than once per second."
    auto timeNow = SystemClock_t::now();

    if ((timeNow - m_LastReadoutTime) 
         >= Seconds_t(MINIMUM_DISPLAY_INTERVAL_SECONDS))
    {
        // Customer Requirement:
        //
        // "2. The displayed temperature shall be the average temperature
        // computed from the latest readings from each node."
        double averageTemperature = 0.0;
        auto count = 0;
        
        for (size_t i = 0; i < g_TheCustomerSensors.size(); i++) 
        {
            // Customer Requirement:
            //
            // "3. In case of intermittent communications, temperature readings older
            // than 10 minutes shall be considered stale and excluded from the 
            // displayed temperature."
            if ((timeNow - g_TheCustomerSensors[i].m_CurrentReadingTime) 
                < std::chrono::minutes(STALE_READING_DURATION_MINUTES))
            {
                if (!g_TheCustomerSensors[i].m_CurrentTemperatureReading.empty())
                {
                    averageTemperature = averageTemperature + 
                      std::stod(g_TheCustomerSensors[i].m_CurrentTemperatureReading);
                    ++count;
                }
            }
        }
        
        // Customer Requirement:
        //
        // "2. The displayed temperature shall be the average temperature
        // computed from the latest readings from each node."
        averageTemperature = averageTemperature / count ;
        
        // You would then use it by creating a local instance of IosFlagSaver
        // whenever you wanted to save the current flag state. When this instance
        // goes out of scope, the flag state will be restored.
        IosFlagSaver iosfs(std::cout);
        
        std::cout << "\t\t" << std::fixed << std::setprecision(1)
                  << averageTemperature << " °C" << "\n";
        
        m_LastReadoutTime = SystemClock_t::now();
    }
}
