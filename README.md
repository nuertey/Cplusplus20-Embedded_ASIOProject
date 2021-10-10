# C++20 EMBEDDED ASIO PROJECT - NUERTEY ODZEYEM SUBMISSION

## PROBLEM STATEMENT:

"We are designing a system for a customer that wants a real-time readout 
of the outside temperature on a display mounted to the wall of their 
meeting room. 

![Customer Readout Illustration](https://github.com/nuertey/RandomArtifacts/blob/master/DisplayReadout.png?raw=true)

The outdoor temperature varies around their site, so they have installed
several temperature data collection systems around their grounds. The 
data collection systems (nodes) are networked via WiFi and send data via
TCP connections. The nodes are already installed, so we do not need to 
provide any software for them. Each node has a static IP, listens on a 
port, accepts a connection, and then sends the latest temperature reading,
in deg C, on one line of ascii text. While the connection remains open,
the node will report the temperature every minute and every time its 
temperature measurement changes by an appreciable amount. In other words,
the frequency that it sends temperature readings is not deterministic.

A display module is already mounted to the wall and wired to an embedded
Linux board via a serial port. Our task is to write a c++ application to
run on this embedded Linux board and write characters to the display by 
sending them to stdout. Assume that the details of the stdout 
communications with the display via serial device are out of scope.  

Also assume that the number of nodes is known at compile time, as are 
the TCP port and IP addresses of the nodes.  In the interest of time, 
do not worry about the low-level details of establishing or 
re-establishing socket connections with the nodes.

The following requirements were agreed upon for the application:
1. The readout shall be as close to real time as possible but shall not
   change faster than once per second.
2. The displayed temperature shall be the average temperature computed
   from the latest readings from each node.
3. In case of intermittent communications, temperature readings older
   than 10 minutes shall be considered stale and excluded from the 
   displayed temperature.
4. If no temperature readings are available, or if the application 
   terminates, the readout shall display “--.- °C”."

## DESIGN OVERVIEW:

 It is assumed that the C++ Standard Template Library (STL)
 and the ASIO C++ Networking Library are available to us.

 Furthermore, I have brought in these two other non-standard header
 files as dependencies:

 [1] My personally developed POSIX threading utilities to aid
 in development and debug of multi-threaded systems. It is
 especially useful for debug as different threads can be named,
 observed and disambiguated via strace, ps, valgrind, gdb, etc.

 [2] Professor M.E. O'Neill's C++11 random number utilities.
 Lacking in the C++ Standard is that namespace encapsulation 
 (randutils::) designed by Prof. O'Neill that ties all the 
 standard C++11 random-number entities and their disparate
 pieces together, in order to make C++11 random number 
 generators straightforward to use without sacrificing any of 
 their power or flexibility.

- https://www.pcg-random.org/posts/ease-of-use-without-loss-of-power.html

Be aware that I have designed things in such a way that most of the socket
operations that occur within this module will occur asynchronously. 

Asynchronicity is the Mother of Speed, Nimbleness and Responsiveness. To 
sate thy righteous Curiosity, the POSIX networking I/O paradigm that 
would be equivalent to ASIO would be epoll().

Kindly step through the code for more extensive comments; especially in
explaining the ASIO io_context usage. For a class diagram depicting the
solution source code, see below:

![Solution Design Class Diagram](https://github.com/nuertey/RandomArtifacts/blob/master/ClassDiagram_detailed.png?raw=true)

## ASIO C++ Library OVERVIEW

I recommend that you glance over the diagram, ASIO_Overview.gif,
so as to familiarize yourself with the structural organization of the 
ASIO C++ Library. That diagram also makes clear the objects and entities
that comprise this useful library.

![ASIO Overview Structural Diagram](https://github.com/nuertey/RandomArtifacts/blob/master/ASIO_Overview.gif?raw=true)

ASIO (Asynchronous I/O) is defined as:   

"Asio is a cross-platform C++ library for network and low-level I/O 
programming that provides developers with a consistent asynchronous 
model using a modern C++ approach.
...

What does Asio provide?

Asio provides the basic building blocks for C++ networking, concurrency
and other kinds of I/O."

- https://think-async.com/Asio/index.html

## CODE DEFINITIONS:
```c++
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

// ...

// Customer Requirement:
//
// "Also assume that the number of nodes is known at compile time, ..."

// Assume some default number of sensor nodes. Feel free though to 
// change the number of sensor nodes when instantiating the template
// for different test scenarios.
using SensorPack_t = std::array<SensorNode_t, 4>;

// ...

    // Initialize all sensor node abstractions.
    for (size_t i = 0; i < m_TheCustomerSensors.size(); i++) 
    {
        // Same test laptop, same LAN, same IP=localhost.
        m_TheCustomerSensors[i].m_Host = SENSOR_NODE_STATIC_IP; 
        
        // Use a different port for each sensor node.
        m_TheCustomerSensors[i].m_Port = std::to_string(EPHEMERAL_PORT_NUMBER_BASE_VALUE + i);
        
        // All operations to occur on ALL the socket connections will
        // occur asynchronously but in the same worker thread context 
        // and on the same Boost.Asio io_context. Asynchronicity will
        // guarantee us the fastest most nimble response. i.e. realtime.
        m_TheCustomerSensors[i].m_pConnectionSocket = std::make_unique<tcp::socket>(Common::g_DispatcherIOContext);
        
        // No temperature reading as yet.
        m_TheCustomerSensors[i].m_TcpData = {}; // Initialize to zeros.
        m_TheCustomerSensors[i].m_CurrentTemperatureReading = "";
        
        // Since no readings exist as yet, default all readings to stale.
        m_TheCustomerSensors[i].m_CurrentReadingTime = SystemClock_t::now()
                - std::chrono::minutes(STALE_READING_DURATION_MINUTES + 1);
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
```

The following sunburst class/object hierarchical diagrams further 
illustrate the solution source code:

![Parent Sunburst Diagram](https://github.com/nuertey/RandomArtifacts/blob/master/Sunburst_Plot-1.png?raw=true)
![TemperatureReadoutApplication Sunburst Diagram](https://github.com/nuertey/RandomArtifacts/blob/master/Sunburst_Plot-5.png?raw=true)
![Utility:: Sunburst Diagram](https://github.com/nuertey/RandomArtifacts/blob/master/Sunburst_Plot-6.png?raw=true)
![SessionManager Sunburst Diagram](https://github.com/nuertey/RandomArtifacts/blob/master/Sunburst_Plot-7.png?raw=true)
![Common:: Sunburst Diagram](https://github.com/nuertey/RandomArtifacts/blob/master/Sunburst_Plot-8.png?raw=true)
![SensorPack_t Sunburst Diagram](https://github.com/nuertey/RandomArtifacts/blob/master/Sunburst_Plot-9.png?raw=true)
![SensorNode_t Sunburst Diagram](https://github.com/nuertey/RandomArtifacts/blob/master/Sunburst_Plot-10.png?raw=true)
![TcpData_t Sunburst Diagram](https://github.com/nuertey/RandomArtifacts/blob/master/Sunburst_Plot-11.png?raw=true)

## TESTING DEFINITIONS:
```c++
static constexpr uint8_t SENSOR_DATA_PERIOD_SECONDS       = 60; // 1 minute = 60 seconds.
static constexpr uint8_t SENSOR_RANDOM_CHANGE_MIN_SECONDS =  1; // 1 second.
static constexpr uint8_t SENSOR_RANDOM_CHANGE_MAX_SECONDS = 60; // 1 minute = 60 seconds.

enum class SensorMode_t : uint8_t
{
    SENSOR_PERIODIC_MODE,
    SENSOR_RANDOM_CHANGE
};

//...

        // Customer Requirement:
        //
        // "While the connection remains open, the node will report the 
        // temperature every minute and every time its temperature measurement
        // changes by an appreciable amount. In other words, the frequency
        // that it sends temperature readings is not deterministic."

        // Be fair in the choice of sensor mode.
        auto currentChoice = Utility::gs_theRNG.pick(
                                  {SensorMode_t::SENSOR_PERIODIC_MODE, 
                                   SensorMode_t::SENSOR_RANDOM_CHANGE});
```

The following sunburst class/object hierarchical diagrams further 
illustrate the Test Artifact source code:

![Parent Sunburst Diagram](https://github.com/nuertey/RandomArtifacts/blob/master/Sunburst_Plot-1.png?raw=true)
![TestArtifactSensorNode Sunburst Diagram](https://github.com/nuertey/RandomArtifacts/blob/master/Sunburst_Plot-2.png?raw=true)
![SensorNodeServer Sunburst Diagram](https://github.com/nuertey/RandomArtifacts/blob/master/Sunburst_Plot-3.png?raw=true)
![TCPSession Sunburst Diagram](https://github.com/nuertey/RandomArtifacts/blob/master/Sunburst_Plot-4.png?raw=true)

## TESTING ASSUMPTIONS:

For testing, and for the Sensor Node application (TestArtifactSensorNode), 
note that I have assumed the following 'interesting' temperature values
range: 
```c++
        // Customer Requirement:
        //
        // "The outdoor temperature varies around their site, so they 
        // have installed several temperature data collection systems 
        // around their grounds".
                
        // For realistic simulation, use a Uniform Distribution model
        // across the standard inhabitable degree Celsius temperature scale.
        auto randomTemperatureReading = Utility::gs_theRNG.uniform(
                                            static_cast<double>(-50.00), 
                                            static_cast<double>(50.00));
```

## LIST OF FILES:
```
.
├── ASIO_Overview.gif
├── ClassDiagram_detailed.png
├── CommonDefinitions.h
├── LICENSE.md
├── meson.build
├── randutils.hpp
├── README.md
├── SessionManager.cpp
├── SessionManager.h
├── Sunburst_Plot-10.png
├── Sunburst_Plot-11.png
├── Sunburst_Plot-1.png
├── Sunburst_Plot-2.png
├── Sunburst_Plot-3.png
├── Sunburst_Plot-4.png
├── Sunburst_Plot-5.png
├── Sunburst_Plot-6.png
├── Sunburst_Plot-7.png
├── Sunburst_Plot-8.png
├── Sunburst_Plot-9.png
├── TemperatureReadoutApplication.cpp
├── TestArtifactSensorNode
│   ├── meson.build
│   └── TestArtifactSensorNode.cpp
└── Threading.h

1 directory, 24 files

```

## DEPENDENCIES:
- meson build frontend - 0.57.2
- ninja build backend - 1.5.1
- C++20
- Linux POSIX Thread Library (libpthread)
- ASIO (standalone version; no BOOST dependencies needed)
- spdlog
- fmt
 
## TESTED ENVIRONMENT:
* Linux 4.10.0-28-generic 
* Ubuntu 16.04 LTS (Xenial Xerus)
* GCC/G++ 11.2.0

## UNTAR:
```
sudo tar -xvf ./NuerteyOdzeyem_C++20_EmbeddedASIO_Project.tar
```

## COMPILATION:
```
rm -rf build
mkdir build
meson build/
ninja -j3 -C build
cd build
```

## EXECUTION EXAMPLES:

[Just 1 Temperature Sensor Node]
```
./TestArtifactSensorNode/TestArtifactSensorNode 5000

./TemperatureReadoutApplication
```

or

[4 Temperature Sensor Nodes]
```
./TestArtifactSensorNode/TestArtifactSensorNode 5000

./TestArtifactSensorNode/TestArtifactSensorNode 5001

./TestArtifactSensorNode/TestArtifactSensorNode 5002

./TestArtifactSensorNode/TestArtifactSensorNode 5003

./TemperatureReadoutApplication
```

## EXECUTION EXAMPLES WITH TEST ARTIFACTS:

1. Open a new terminal to represent a Temperature Sensor Node. Issue the following command:
```
./TestArtifactSensorNode/TestArtifactSensorNode 5000

    Output:
    
    Spawning boost::asio::io_context... 
    Constructing SensorNodeServer listening on port... [5000]
    Waiting to accept TCP connections... 
    TCP session established with TemperatureReadoutApplication on port :-> 5000.
    Constructing TCP Session... 
    About to send temperature reading to TemperatureReadoutApplication... 
    About to send temperature reading to TemperatureReadoutApplication... 
    About to send temperature reading to TemperatureReadoutApplication... 
    About to send temperature reading to TemperatureReadoutApplication... 
    About to send temperature reading to TemperatureReadoutApplication... 
    About to send temperature reading to TemperatureReadoutApplication... 
    About to send temperature reading to TemperatureReadoutApplication... 
    About to send temperature reading to TemperatureReadoutApplication... 
    About to send temperature reading to TemperatureReadoutApplication... 
    About to send temperature reading to TemperatureReadoutApplication... 
```

2. On the original compile window, designate that that represents the Temperature Readout Application. Issue the following command: 
```
./TemperatureReadoutApplication

    Output:
    
            --.- °C
    [INFO] : Parent just created a thread.                ThreadName = WorkerThread_fs
    [DEBUG] Connecting to TCP endpoint :-> 127.0.0.1:5000
    [TRACE] ALL temperature sensor nodes have been successfully connected to.
    
            38.7 °C
    
            10.7 °C
    
            26.1 °C
    
            44.0 °C
    
            42.9 °C
    
            -26.7 °C
    
            27.9 °C

```
4. The algorithm should then begin accepting temperature readings and 
exercising the business logic to display to the user per the customer 
requirements. Examine each relevant terminal (especially the Temperature
Readout Application) for the appropriate output.

## EXECUTION OUTPUT WITH 4 TEMPERATURE SENSOR NODES:

```
./TemperatureReadoutApplication

    Output:
    
    [INFO] : Parent just created a thread.                ThreadName = WorkerThread_eT
            --.- °C
    [DEBUG] Connecting to TCP endpoint :-> 127.0.0.1:5000
    [DEBUG] Connecting to TCP endpoint :-> 127.0.0.1:5001
    [DEBUG] Connecting to TCP endpoint :-> 127.0.0.1:5002
    [DEBUG] Connecting to TCP endpoint :-> 127.0.0.1:5003
    [TRACE] ALL temperature sensor nodes have been successfully connected to.
            -15.3 °C
            -9.2 °C
            -17.0 °C
            3.0 °C
            6.8 °C
            15.9 °C
            22.0 °C
            13.3 °C
            26.9 °C
            29.3 °C
            27.1 °C
            22.4 °C
            21.5 °C
            4.8 °C
            5.4 °C
            21.4 °C
            10.0 °C
            21.7 °C
            4.6 °C
            -4.3 °C
            -10.9 °C
            -15.1 °C
            -14.8 °C
            -16.3 °C
            1.2 °C
            -15.5 °C
            -20.6 °C
            -21.2 °C
            -12.1 °C
            -11.8 °C
            4.9 °C
            -14.3 °C
            -12.7 °C
            -10.0 °C
            -0.6 °C
            -15.1 °C
            -13.1 °C
            0.6 °C
            -16.3 °C
            -5.2 °C
            -0.2 °C
            4.8 °C
            -0.5 °C
            4.9 °C
            11.8 °C
            -2.1 °C
            -0.0 °C
            9.6 °C
            -2.1 °C
            10.0 °C
```
 
## EXIT:

The application catches the following signals so either can be used to 
halt processing :
```
      SIGINT  - the interrupt signal (user presses 'ctrl-c')
      SIGQUIT - the dump core signal (user presses 'ctrl-\')
      SIGTERM - default signal for kill command (kill $PID)
```

## WARNING:

1) Be aware that if the program is forcibly halted whilst the SessionManager
is still constructing and connecting to the sockets, then by design,
the program will throw an exception before exiting. Logically, that 
exception would be: 
```
what():  connect: Interrupted system call
```

Copyright (c) 2021 Nuertey Odzeyem. All Rights Reserved.
