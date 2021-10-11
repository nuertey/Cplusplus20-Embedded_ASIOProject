#include "CommonDefinitions.h"

static constexpr uint8_t SENSOR_DATA_PERIOD_SECONDS       = 60; // 1 minute = 60 seconds.
static constexpr uint8_t SENSOR_RANDOM_CHANGE_MIN_SECONDS =  1; // 1 second.
static constexpr uint8_t SENSOR_RANDOM_CHANGE_MAX_SECONDS = 60; // 1 minute = 60 seconds.

enum class SensorMode_t : uint8_t
{
    SENSOR_PERIODIC_MODE,
    SENSOR_RANDOM_CHANGE
};
    
class TCPSession : public std::enable_shared_from_this<TCPSession>
{
public:
    TCPSession(tcp::socket socket)
        : m_Socket(std::move(socket))
    {
        std::cout << "Constructing TCP Session... \n";
    }

    void Start()
    {
        DoWrite();
    }

private:
    void ComposeTemperature()
    {       
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
                                            
        // Customer Requirement:
        //
        // "Each node has a static IP, listens on a port, accepts a connection,
        // and then sends the latest temperature reading, in deg C, on one line
        // of ascii text."
        auto temperatureString = std::to_string(randomTemperatureReading);

        // Send current temperature reading to the TemperatureReadoutApplication.
        std::cout << "About to send temperature reading to TemperatureReadoutApplication... \n";
        asio::write(m_Socket, asio::buffer(temperatureString.data(), 
                                           temperatureString.size()));        
 
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

        uint8_t holdoffTime = 0;
        if (SensorMode_t::SENSOR_PERIODIC_MODE == currentChoice)
        {
            holdoffTime = SENSOR_DATA_PERIOD_SECONDS;
        }
        else if (SensorMode_t::SENSOR_RANDOM_CHANGE == currentChoice)
        {
            auto randomDurationForNextTempChange = Utility::gs_theRNG.uniform(
                  static_cast<uint8_t>(SENSOR_RANDOM_CHANGE_MIN_SECONDS), 
                  static_cast<uint8_t>(SENSOR_RANDOM_CHANGE_MAX_SECONDS - 1));
                  
            holdoffTime = randomDurationForNextTempChange;
        }
        
        // Holdoff till next sensor acquisition iteration time.
        std::this_thread::sleep_for(Seconds_t(holdoffTime));
    }

    void DoWrite()
    {
        auto self(shared_from_this());
         
        ComposeTemperature();
        
        DoWrite();
    }

    tcp::socket m_Socket;
};

// As a testing tool, simulate the temperature data collection system
// installed around the customer's grounds.
class SensorNodeServer
{
public:
    SensorNodeServer(asio::io_context& io_context, short port)
        : m_Acceptor(io_context, tcp::endpoint(tcp::v4(), port))
        , m_PortNumber(port)
    {
        std::cout << "Constructing SensorNodeServer listening on port... [" << port << "]\n";
        DoAccept();
    }

private:
    void DoAccept()
    {
        std::cout << "Waiting to accept TCP connections... \n";

        m_Acceptor.async_accept(
            [this](std::error_code ec, tcp::socket socket)
            {
                if (!ec)
                {
                    std::cout << "TCP session established with TemperatureReadoutApplication on port :-> " 
                              << m_PortNumber << ".\n";
                    // TBD Nuertey Odzeyem; note here that this shared_ptr
                    // temporary paradigm is unsafe due to object lifetimes,
                    // and only works here because technically Start() is
                    // recursive and we never return. Fix it!
                    //std::make_shared<TCPSession>(std::move(socket))->Start();
                    
                    auto theSession = std::make_shared<TCPSession>(std::move(socket));
                    theSession->Start();
                }

                DoAccept();
            });
    }

    tcp::acceptor m_Acceptor;
    short         m_PortNumber;
};

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: TestArtifactSensorNode <port>\n\n";
            return 1;
        }

        std::cout << "Spawning asio::io_context... \n";
        asio::io_context io_context;
        SensorNodeServer s(io_context, std::atoi(argv[1]));

        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
