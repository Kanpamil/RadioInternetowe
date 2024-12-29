#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <thread>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <vector>
#include <mutex>
#include <queue>
#include <signal.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#define PORT 1100
#define CHUNK_SIZE 4096// 64 KB per chunk for slower streaming
char buffer[CHUNK_SIZE];
std::vector<int> clientStreamSocket;
std::mutex clientSocketMutex;
std::vector<std::string> fileQueue;
int serverSocket = -1;
bool running = true;

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nCaught SIGINT (Ctrl+C), shutting down...\n";
        
        // Close all client sockets
        for (int clientSocket : clientStreamSocket) {
            close(clientSocket);
        }
        // Close the server socket if it's open
        if (serverSocket != -1) {
            close(serverSocket);
            std::cout << "Server socket closed.\n";
        }
        
        // Set the flag to stop the server loop
        running = false;
    }
}

// Function to stream MP3 data to a client
void streamMP3(const char* filename, int clientSocket) {
    av_register_all();

    AVFormatContext* formatContext = nullptr;

    // Open the input file
    if (avformat_open_input(&formatContext, filename, nullptr, nullptr) != 0) {
        std::cerr << "Failed to open MP3 file." << std::endl;
        return;
    }

    // Find stream info
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        std::cerr << "Failed to find stream info." << std::endl;
        avformat_close_input(&formatContext);
        return;
    }

    // Get the codec parameters (e.g., bitrate) to estimate the playback time per packet
    AVCodecParameters* codecParams = formatContext->streams[0]->codecpar;
    int bitrate = codecParams->bit_rate;  // in bits per second (bps)
    double packetDuration = 0.0;

    if (bitrate > 0) {
        // Time to play 627 bytes (in seconds)
        packetDuration = (627 * 8) / static_cast<double>(bitrate);  // Duration in seconds
    } else {
        std::cerr << "Bitrate not available. Assuming 128kbps." << std::endl;
        packetDuration = (627 * 8) / 128000.0;  // Default to 128kbps if bitrate is unknown
    }

    // Read packets and send them to the client
    AVPacket packet;
    av_init_packet(&packet);

    // Simulate real-time streaming
    while (av_read_frame(formatContext, &packet) >= 0) {
        ssize_t bytesSent = send(clientSocket, packet.data, packet.size, 0);
        if (bytesSent < 0) {
            std::cerr << "Error sending data to client." << std::endl;
            break;
        }
        av_packet_unref(&packet);

        // Simulate real-time streaming by waiting for the estimated packet duration
        std::this_thread::sleep_for(std::chrono::duration<double>(packetDuration));
    }

    avformat_close_input(&formatContext);
    close(clientSocket);
}


// Function to handle multiple clients and stream data
void handleClient(int clientSocket, const std::string& filename) {
    streamMP3(filename.c_str(), clientSocket);
}

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGPIPE, SIG_IGN);

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Error creating socket." << std::endl;
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Bind failed." << std::endl;
        close(serverSocket);
        return 1;
    }

    if (listen(serverSocket, 5) < 0) {
        std::cerr << "Listen failed." << std::endl;
        close(serverSocket);
        return 1;
    }

    std::cout << "Server listening on port " << PORT << "..." << std::endl;

    sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    // Load MP3 files into queue
    std::cout << "Loading files to queue..." << std::endl;
    for (auto& file : std::filesystem::directory_iterator("./mp3files")) {
        fileQueue.push_back(file.path());
    }

    while (running) {
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            std::cerr << "Failed to accept client." << std::endl;
            continue;
        }

        std::cout << "Client connected." << std::endl;
        std::cout << "Client IP: " << inet_ntoa(clientAddr.sin_addr) << std::endl;
        std::cout << "Client port: " << ntohs(clientAddr.sin_port) << std::endl;

        // Handle the client in a separate thread
        std::thread(handleClient, clientSocket, fileQueue[0]).detach();
    }

    std::cout << "Server shutting down..." << std::endl;
    return 0;
}
