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

#define PORT 1100
#define CHUNK_SIZE 2048

int getBitrate(const std::string& filePath) {
    TagLib::FileRef file(filePath.c_str());
    if (!file.isNull() && file.audioProperties()) {
        return file.audioProperties()->bitrate();
    }
    return 128;  // Default to 128 kbps if bitrate cannot be determined
}

void streamMP3File(const std::string& filePath, int clientSocket, int bitrate) {
    std::ifstream mp3File(filePath, std::ios::binary);
    if (!mp3File) {
        std::cerr << "Failed to open MP3 file: " << filePath << std::endl;
        return;
    }

    int bytesPerSecond = bitrate > 0 ? bitrate * 1000 / 8 : 128 * 1000 / 8;  // Default to 128 kbps
    float timePerChunk = static_cast<float>(CHUNK_SIZE) / bytesPerSecond;   // Time for each chunk in seconds
    auto sleepTimeMicros = std::chrono::microseconds(static_cast<int>(timePerChunk * 1e6));

    char buffer[CHUNK_SIZE];
    while (mp3File.read(buffer, CHUNK_SIZE) || mp3File.gcount() > 0) {
        size_t bytesRead = mp3File.gcount();

        // Send data to the client
        size_t totalSent = 0;
        while (totalSent < bytesRead) {
            ssize_t bytesSent = send(clientSocket, buffer + totalSent, bytesRead - totalSent, 0);
            // if (bytesSent < 0) {
            //     std::cerr << "Failed to send data to client: " << strerror(errno) << std::endl;
            //     mp3File.close();
            //     return;
            // }
            totalSent += bytesSent;
        }

        // Delay to simulate real-time streaming
        std::this_thread::sleep_for(sleepTimeMicros);
    }

    mp3File.close();
}

void printMP3Metadata(const std::string& filePath) {
    TagLib::FileRef file(filePath.c_str());

    if (!file.isNull() && file.tag()) {
        TagLib::Tag *tag = file.tag();

        std::cout << "Title: " << tag->title() << std::endl;
        std::cout << "Artist: " << tag->artist() << std::endl;
        std::cout << "Album: " << tag->album() << std::endl;
        std::cout << "Year: " << tag->year() << std::endl;
        std::cout << "Comment: " << tag->comment() << std::endl;
        std::cout << "Genre: " << tag->genre() << std::endl;
    }

    if (!file.isNull() && file.audioProperties()) {
        TagLib::AudioProperties *properties = file.audioProperties();

        std::cout << "Bitrate: " << properties->bitrate() << " kbps" << std::endl;
        std::cout << "Sample Rate: " << properties->sampleRate() << " Hz" << std::endl;
        std::cout << "Channels: " << properties->channels() << std::endl;
        std::cout << "Length: " << properties->length() << " seconds" << std::endl;
    }
}

int main() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
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

    std::cout << "Server listening on port " << PORT << "..." << std::endl;

    sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
    if (clientSocket < 0) {
        std::cerr << "Failed to accept client." << std::endl;
        close(serverSocket);
        return 1;
    }

    std::cout << "Client connected." << std::endl;

    std::string mp3FilePath = "./mp3files/kerry.mp3";  // Path to the MP3 file to stream

    // Print metadata
    printMP3Metadata(mp3FilePath);

    // Stream MP3 file
    int bitrate = getBitrate(mp3FilePath);

    streamMP3File(mp3FilePath, clientSocket, bitrate);

    close(clientSocket);
    close(serverSocket);
    return 0;
}
