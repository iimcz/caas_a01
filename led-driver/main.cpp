#include <iostream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <chrono>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <csignal>
#include <cstring>

#include <libconfig.h++>

#include "LedDriver.h"

std::unique_ptr<LedDriver> ledDriver;
std::unique_ptr<std::thread> driverThread;
std::atomic_bool driverThreadRunning(true);
std::atomic_bool canExit(false);
int sockfd = 0;

void exitHandler(int signal)
{
    std::cout << "Exiting..." << std::endl;
    if (sockfd > 0)
    {
        std::cout << "Stopping the control socket...";
        close(sockfd);
        std::cout << "DONE" << std::endl;
    }
    if (driverThread)
    {
        std::cout << "Stopping the rendering thread...";
        driverThreadRunning = false;
        driverThread->join();
        std::cout << "DONE" << std::endl;
    }
    if (ledDriver)
    {
        std::cout << "Clearing leds...";
        ledDriver->clear();
        std::cout << "DONE" << std::endl;
    }
    canExit = true;
}

inline color_t colorFromInteger(uint32_t src)
{
    return color_t{
        .r = static_cast<uint8_t>(src & 0xFF000000 >> 24),
        .g = static_cast<uint8_t>(src & 0x00FF0000 >> 16),
        .b = static_cast<uint8_t>(src & 0x0000FF00 >> 8),
        .w = static_cast<uint8_t>(src & 0x000000FF)};
}

void printPalette(color_t primary, color_t secondary, color_t fill, const char *prefix = "")
{
    std::cout << prefix;
    std::cout << "Primary RGBW: " << primary.r << " " << primary.g << " " << primary.b << " " << primary.w << std::endl;

    std::cout << prefix;
    std::cout << "Secondary RGBW: " << secondary.r << " " << secondary.g << " " << secondary.b << " " << secondary.w << std::endl;

    std::cout << prefix;
    std::cout << "Fill RGBW: " << fill.r << " " << fill.g << " " << fill.b << " " << fill.w << std::endl;
}

void receiveControl()
{
    char buffer[512];
    sockaddr_in remote;
    socklen_t remoteSize = sizeof(remote);
    ssize_t bytes = recvfrom(sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr *)&remote, &remoteSize);

    if (bytes < 0)
    {
        if (errno == EBADF)
        {
            // Socket got closed. Just exit.
            return;
        }
        std::cout << "Error receiving a control message: " << errno << std::endl;
        exitHandler(SIGINT);
        return;
    }
    std::cout << "Received a control message from " << inet_ntoa(remote.sin_addr) << std::endl;

    std::string message(buffer, strnlen(buffer, sizeof(buffer)));

    std::string cmd = message.substr(0, message.find_first_of(' '));
    std::string arg = message.substr(cmd.length() + 1);
    std::cout << "Command: " << cmd << std::endl
              << "Argument: " << arg << std::endl;

    if (cmd == "AdvanceStagePulsing")
    {
        int num = atoi(arg.c_str());
        if (num <= AnimStage::kFade && num >= AnimStage::kDark)
        {
            std::cout << "Starting pulsing." << std::endl;
            ledDriver->setPulsing(true);
            ledDriver->advanceStage(static_cast<AnimStage>(num));
        }
    }
    if (cmd == "AdvanceStage")
    {
        if (ledDriver->getPulsing())
        {
            std::cout << "Ignoring non-pulsing state change until the pulsing cycle finishes." << std::endl;
            return;
        }
        int num = atoi(arg.c_str());
        if (num <= AnimStage::kFade && num >= AnimStage::kDark)
        {
            ledDriver->advanceStage(static_cast<AnimStage>(num));
        }
    }
    else if (cmd == "Palette")
    {
        std::stringstream ss(arg);
        int r, g, b, w;
        ss >> r;
        ss >> g;
        ss >> b;
        ss >> w;
        color_t primary = {
            .r = static_cast<uint8_t>(r),
            .g = static_cast<uint8_t>(g),
            .b = static_cast<uint8_t>(b),
            .w = static_cast<uint8_t>(w)};

        ss >> r;
        ss >> g;
        ss >> b;
        ss >> w;
        color_t secondary = {
            .r = static_cast<uint8_t>(r),
            .g = static_cast<uint8_t>(g),
            .b = static_cast<uint8_t>(b),
            .w = static_cast<uint8_t>(w)};

        ss >> r;
        ss >> g;
        ss >> b;
        ss >> w;
        color_t fill = {
            .r = static_cast<uint8_t>(r),
            .g = static_cast<uint8_t>(g),
            .b = static_cast<uint8_t>(b),
            .w = static_cast<uint8_t>(w)};

        std::cout << "Setting palette to: " << std::endl;
        printPalette(primary, secondary, fill, "\t");
        ledDriver->setColorScheme(primary, secondary, fill);
    }
}

void renderThread()
{
    ledDriver->finalize();

    const float targetFrameRate = 30.0f;
    const std::chrono::duration<float> targetFrameTime(1.0f / targetFrameRate);

    // Variables for tracking time
    std::chrono::high_resolution_clock::time_point previousTime = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point currentTime;

    // Start the update loop
    while (ledDriver && driverThreadRunning)
    {
        // Calculate delta time
        currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> deltaTime = currentTime - previousTime;
        previousTime = currentTime;

        ledDriver->update(deltaTime.count());
        ledDriver->render();

        // Delay to achieve desired frame rate
        std::this_thread::sleep_for(targetFrameTime - (currentTime - std::chrono::high_resolution_clock::now()));
    }
}

int main(int argc, char *argv[])
{
    std::signal(SIGINT, exitHandler);

    libconfig::Config config;
    if (argc >= 2)
    {
        std::cout << "Reading config file...";
        try
        {
            config.readFile(argv[1]);
        }
        catch (libconfig::ParseException pe)
        {
            std::cout << "FAIL! parsing error: " << pe.what() << std::endl;
        }
        catch (libconfig::FileIOException ioe)
        {
            std::cout << "FAIL! i/o error: " << ioe.what() << std::endl;
        }
        std::cout << "DONE" << std::endl;
    }

    std::cout << "Creating an UDP control socket...";
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        std::cout << "FAIL! error creating a socket: " << errno << std::endl;
        return 0;
    }
    uint32_t control_port = 13798;
    config.lookupValue("control_port", control_port);
    sockaddr_in address{
        .sin_family = AF_INET,
        .sin_port = htons(control_port),
        .sin_addr = {.s_addr = INADDR_ANY}};
    if (bind(sockfd, (struct sockaddr *)(&address), sizeof(address)) < 0)
    {
        std::cout << "FAIL! error in bind: " << errno << std::endl;
        close(sockfd);
        return 0;
    }
    std::cout << "DONE" << std::endl;

    std::cout << "Starting the rendering thread...";
    ledDriver = std::make_unique<LedDriver>();
    ledDriver->applyConfig(config);
    driverThread = std::make_unique<std::thread>(renderThread);
    std::cout << "DONE" << std::endl;

    std::cout << "Now listening for control messages." << std::endl;
    while (driverThreadRunning)
    {
        receiveControl();
    }

    while (!canExit)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}