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



#define PORT 1100
#define CHUNK_SIZE 4096
char buffer[CHUNK_SIZE];
std::vector<int> clientStreamSocket;
std::mutex clientSocketMutex;
std::vector<std::string> fileQueue;
int serverSocket = -1;
int clientSocket = -1;
bool running = true;

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nCaught SIGINT (Ctrl+C), shutting down...\n";
        
        // Close the client socket if it's open
        if (clientSocket != -1) {
            close(clientSocket);
            std::cout << "Client socket closed.\n";
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
void sendFileQueue(int clientSocket, const std::vector<std::string>& fileQueue) {
    std::string queueData;
    for (const auto& file : fileQueue) {
        queueData += file + "\n"; // Concatenate file names with newline as delimiter
    }
    send(clientSocket, queueData.c_str(), queueData.size(), 0);
}

// void sendMetadata(int clientSocket, const std::string& filePath) {
//     TagLib::FileRef file(filePath.c_str());
//     std::ostringstream metadata;

//     if (!file.isNull() && file.tag()) {
//         TagLib::Tag *tag = file.tag();
//         metadata << "Title: " << tag->title() << "\n";
//         metadata << "Artist: " << tag->artist() << "\n";
//         metadata << "Album: " << tag->album() << "\n";
//         metadata << "Year: " << tag->year() << "\n";
//         metadata << "Comment: " << tag->comment() << "\n";
//         metadata << "Genre: " << tag->genre() << "\n";
//     }

//     if (!file.isNull() && file.audioProperties()) {
//         TagLib::AudioProperties *properties = file.audioProperties();
//         metadata << "Bitrate: " << properties->bitrate() << " kbps\n";
//         metadata << "Sample Rate: " << properties->sampleRate() << " Hz\n";
//         metadata << "Channels: " << properties->channels() << "\n";
//         metadata << "Length: " << properties->length() << " seconds\n";
//     }

//     std::string metadataStr = metadata.str();
//     std::cout << "Sending metadata to client: " << metadataStr << std::endl;
//     send(clientSocket, metadataStr.c_str(), metadataStr.size(), 0);
// }

// int getBitrate(const std::string& filePath) {
//     TagLib::FileRef file(filePath.c_str());
//     if (!file.isNull() && file.audioProperties()) {
//         return file.audioProperties()->bitrate();
//     }
//     return 128;  // Default to 128 kbps if bitrate cannot be determined
// }

// void streamMP3File(const std::string& filePath, int clientSocket){
//     int bitrate = getBitrate(filePath);
//     std::ifstream mp3File(filePath, std::ios::binary);
//     if (!mp3File) {
//         std::cerr << "Failed to open MP3 file: " << filePath << std::endl;
//         return;
//     }

//     int bytesPerSecond = bitrate > 0 ? bitrate * 1000 / 8 : 128 * 1000 / 8;  // Default to 128 kbps
//     float timePerChunk = static_cast<float>(CHUNK_SIZE) / bytesPerSecond;   // Time for each chunk in seconds
//     auto sleepTimeMicros = std::chrono::microseconds(static_cast<int>(timePerChunk * 1e6));

//     char buffer[CHUNK_SIZE];
//     while (mp3File.read(buffer, CHUNK_SIZE) || mp3File.gcount() > 0) {
//         // Send data to the client
//         ssize_t bytesSent = send(clientSocket, buffer, sizeof(buffer), 0);
//         if (bytesSent < 0) {
//             if (errno == EPIPE) {
//                 std::cerr << "Client disconnected. EPIPE error encountered." << std::endl;
//                 break;  // Stop further attempts to send to this client
//             } 
//             else {
//                 std::cerr << "Error sending data: " << strerror(errno) << std::endl;
//             }
//         }

//         // Delay to simulate real-time streaming
//         std::this_thread::sleep_for(sleepTimeMicros);
//     }

//     mp3File.close();
// }

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

    if (listen(serverSocket, 1) < 0) {
        std::cerr << "Listen failed." << std::endl;
        close(serverSocket);
        return 1;
    }
    std::cout << "Loading files to queue" << std::endl;
    for(auto file : std::filesystem::directory_iterator("./mp3files")){
        fileQueue.push_back(file.path());
    }

    std::cout << "Server listening on port " << PORT << "..." << std::endl;

    sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    while(running){
        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            std::cerr << "Failed to accept client." << std::endl;
            continue;
        }
        clientStreamSocket.push_back(clientSocket);
        std::cout << "Client connected." << std::endl;
        std::cout << "Client IP: " << inet_ntoa(clientAddr.sin_addr) << std::endl;
        std::cout << "Client port: " << ntohs(clientAddr.sin_port) << std::endl;
        // sendFileQueue(clientSocket, fileQueue);
        // sendMetadata(clientSocket, fileQueue[0]);
        // streamMP3File(fileQueue[0], clientSocket);
        std::cout << "Streaming ended" << std::endl;
        close(clientSocket);
    }
    std::cout << "Server shutting down..." << std::endl;
}






// void fileStreaming(const std::string& filePath, std::vector<int> &clientSocket, int bitrate) {
//     std::ifstream mp3File(filePath, std::ios::binary);
//     if (!mp3File) {
//         std::cerr << "Failed to open MP3 file: " << filePath << std::endl;
//         return;
//     }
//     int bytesPerSecond = bitrate > 0 ? bitrate * 1000 / 8 : 128 * 1000 / 8;  // Default to 128 kbps
//     float timePerChunk = static_cast<float>(CHUNK_SIZE) / bytesPerSecond;   // Time for each chunk in seconds
//     auto sleepTimeMicros = std::chrono::microseconds(static_cast<int>(timePerChunk * 1e6));

//     while (mp3File.read(buffer, CHUNK_SIZE) || mp3File.gcount() > 0) {
//         size_t bytesRead = mp3File.gcount();
//         for(auto clientSocket : clientStreamSocket){
//             ssize_t bytesSent = send(clientSocket, buffer, bytesRead, 0);
//             if (bytesSent < 0) {
//                 std::cerr << "Failed to send data to client: " << strerror(errno) << std::endl;
//                 mp3File.close();
//                 return;
//             }
//         }
//         // Delay to simulate real-time streaming
//         std::this_thread::sleep_for(sleepTimeMicros);
//     }
//     mp3File.close();
    

// }