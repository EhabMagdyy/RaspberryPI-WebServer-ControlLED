/*************************************************************************************************************
**************************    Author  : Ehab Magdy Abdullah                      *****************************
**************************    Linkedin: https://www.linkedin.com/in/ehabmagdyy/  *****************************
**************************    Youtube : https://www.youtube.com/@EhabMagdyy      *****************************
**************************************************************************************************************/

// GPIO Libs
#include <gpiod.h>
#include <iostream>
#include <csignal>
#include <thread> 
#include <chrono>
#include <memory>

// Raspberry PI Web Server Libs
#include <boost/asio.hpp>       // sudo apt install libboost-all-dev
#include <iostream>
#include <sstream>
#include <string>
// Raspberry PI Web Server namescpes
using namespace boost::asio;
using ip::tcp;
using std::cout;
using std::endl;
using std::string;

// I'm using GPIO PIN 22
#define LED_PIN          22
#define CONSUMER_NAME    "WebServeLedPin"

// Web Server Port
#define PORT_NUMBER       5555

// hold button state: 1 for ON, 0 for OFF.
int button_pressed = 0;

/********************************** LED Control :GPIO Config *********************************/
// LedController Class
class LedController
{
private:
    int pin_;
    std::string consumer_name_;
    std::unique_ptr<gpiod_chip, decltype(&gpiod_chip_close)> chip_;
    std::unique_ptr<gpiod_line, decltype(&gpiod_line_release)> line_;
    bool cleaned_up_ = false;

    void getLine() {
        line_ = std::unique_ptr<gpiod_line, decltype(&gpiod_line_release)>(
            gpiod_chip_get_line(chip_.get(), pin_), gpiod_line_release);
        if (!line_) {
            throw std::runtime_error("Failed to get GPIO line");
        }
    }

    void requestLine() {
        if (gpiod_line_request_output(line_.get(), consumer_name_.c_str(), 0) < 0) {
            throw std::runtime_error("Failed to request GPIO line as output");
        }
    }

    void setLineValue(int value) {
        if (gpiod_line_set_value(line_.get(), value) < 0) {
            throw std::runtime_error("Failed to set GPIO line value");
        }
    }

public:
    LedController(int pin, const std::string& consumer_name)
        : pin_(pin), consumer_name_(consumer_name),
          chip_(gpiod_chip_open_by_name("gpiochip0"), gpiod_chip_close),
          line_(nullptr, gpiod_line_release) {
        if (!chip_) {
            throw std::runtime_error("Failed to open gpiochip0");
        }
        getLine();
        requestLine();
    }

    ~LedController() {
        if(!cleaned_up_){
            cleanup();
        }
    }

    void SetLedStatus(int val) {
        setLineValue(val);
    }

    void cleanup() {
        if (!cleaned_up_) {
            if (line_) {
                setLineValue(0);
                gpiod_line_release(line_.get());
            }
            if (chip_) {
                gpiod_chip_close(chip_.get());
            }
            std::cout << "\nGPIO cleanup done. Exiting safely." << std::endl;
            cleaned_up_ = true;
        }
    }
};

std::unique_ptr<LedController> ledController;

void signalHandler(int sig) {
    if (ledController) {
        ledController->cleanup();
    }
    std::exit(0);
}

/**************************************************************************************/

/************************************* Web Server *************************************/

string read_(tcp::socket &socket)
{
    boost::asio::streambuf buf;
    boost::asio::read_until(socket, buf, "\r\n");
    string data = boost::asio::buffer_cast<const char *>(buf.data());
    return data;
}

void send_(tcp::socket &socket, const string &message)
{
    boost::asio::write(socket, boost::asio::buffer(message));
}

/**************************************************************************************/

int main()
{
    try
    {
        boost::asio::io_service io_service;
        tcp::acceptor acceptor_(io_service, tcp::endpoint(tcp::v4(), PORT_NUMBER));

        cout << "Server running on port " << PORT_NUMBER << "...\n";

        // Setup Led pin
        std::signal(SIGINT, signalHandler);
        ledController = std::make_unique<LedController>(LED_PIN, CONSUMER_NAME);

        while (true)
        {
            tcp::socket socket_(io_service);
            acceptor_.accept(socket_);

            // Read the first line of the HTTP request (e.g., "GET / HTTP/1.1")
            string request_line = read_(socket_);
            cout << "Client Request: " << request_line << endl;

            // Parse the request line to get the path.
            std::istringstream request_stream(request_line);
            string method, path, protocol;
            request_stream >> method >> path >> protocol;

            // If the request is for the command (/send?message=on or /send?message=off)
            if (path.find("/send?message=") == 0)
            {
                // Extract the message value.
                string msg_value = path.substr(string("/send?message=").length());
                if (msg_value == "on")
                {
                    button_pressed = 1;
                }
                else if (msg_value == "off")
                {
                    button_pressed = 0;
                }
                // Prepare a plain text response indicating the value of button_pressed.
                string response_body = "button_pressed=" + std::to_string(button_pressed);
                string response = "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: text/plain\r\n"
                                  "Content-Length: " + std::to_string(response_body.size()) + "\r\n"
                                  "Connection: close\r\n\r\n" +
                                  response_body;
                send_(socket_, response);
            }
            else
            {
                // Otherwise, serve the HTML page with the ON/OFF buttons.
                string html_response = R"HTML(HTTP/1.1 200 OK
                    Content-Type: text/html
                    Connection: close

                    <!DOCTYPE html>
                    <html lang="en">
                    <head>
                        <meta charset="UTF-8">
                        <meta name="viewport" content="width=device-width, initial-scale=1.0">
                        <title>Ehab RPI Control</title>
                        <style>
                            body { font-family: Arial, sans-serif; text-align: center; background-color: #f4f4f4; margin: 0; padding: 0; }
                            .container { width: 50%; margin: auto; background: white; padding: 20px; margin-top: 50px; box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.1); border-radius: 8px; }
                            h1 { color: #333; }
                            p { font-size: 18px; color: #555; }
                            button { padding: 10px 20px; font-size: 18px; color: white; border: none; border-radius: 5px; cursor: pointer; margin: 10px; }
                            .on { background-color: #28a745; }
                            .off { background-color: #dc3545; }
                            button:hover { opacity: 0.8; }
                        </style>
                    </head>
                    <body>
                        <div class="container">
                            <h1>Raspberry Pi</h1>
                            <p>Control LED</p>
                            <button class="on" onclick="sendMessage('on')">ON</button>
                            <button class="off" onclick="sendMessage('off')">OFF</button>
                            <p id="response"></p>
                        </div>
                        <script>
                            function sendMessage(value) {
                                fetch("/send?message=" + value)
                                .then(response => response.text())
                                .then(data => {
                                    document.getElementById("response").innerText = "Server Response: " + data;
                                })
                                .catch(error => console.error("Error:", error));
                            }
                        </script>
                    </body>
                    </html>
                    )HTML";
                send_(socket_, html_response);
            }
            cout << "Response sent. Current button_pressed = " << button_pressed << endl;

            ledController->SetLedStatus(button_pressed);
        }
    }
    catch (std::exception &e)
    {
        std::cerr << "Server error: " << e.what() << std::endl;
    }
    return 0;
}
