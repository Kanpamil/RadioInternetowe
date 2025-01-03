#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <thread>
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

#define CONTROL_PORT 1100
#define STREAM_PORT 1101
#define CHUNK_SIZE 4096
#define LUI long unsigned int

std::vector<int> streamingSockets;
std::mutex clientSocketMutex;
std::mutex momentLock;
std::mutex fileLock;
std::vector<std::string> fileQueue;
std::string streamedFile;
int trackMoment = 0;
int serverControlSocket = -1;
int serverStreamSocket = -1;
bool running = true;

void signalHandler(int signal);


void save_file(const std::string& file_name, const char* data, size_t size);

void get_file(int client_socket, std::string file_name);

std::string get_name(int client_socket);

void sendFileQueue(int clientSocket){
    std::string queue = "";
    for(LUI i = 0; i < fileQueue.size(); i++) {
        std::string name = fileQueue[i];
        name = name.substr(name.find_last_of("/") + 1);
        std::cout << "File: " << name << std::endl;
        queue += name + "\n";
    }
    ssize_t bytes_sent = send(clientSocket, queue.c_str(), queue.length(), 0);
    if(bytes_sent < 0) {
        std::cerr << "Error sending file queue" << std::endl;
    }
    std::cout << "File queue sent: " << bytes_sent << std::endl;
}
void streamTracks();

// Function to stream MP3 data to a client
void sendMP3FileToClient(int clientStreamingSocket, const std::string& filename) {
    char message[CHUNK_SIZE]; // Buffer for messages
    bzero(message,CHUNK_SIZE);
    std::ifstream file(filename, std::ios::binary); // Open the file in binary mode
    
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }
    file.seekg(0, std::ios::end);  // Move to the end
    std::streamsize fileSize = 0;
    fileSize = file.tellg();  // Get file size
    file.seekg(0, std::ios::beg);  // Return to the beginning
    // Send the file size to the client
    std::string size = std::to_string(fileSize);
    std::cout << "Size of file: "<< size << std::endl; 
    ssize_t bytes_sent = send(clientStreamingSocket, size.c_str(), size.length(), 0);
    if (bytes_sent < 0) {
        std::cerr << "Error sending file size to client." << std::endl;
        return;
    }
    else{
        std::cout << "File size sent to client." << std::endl;
    }
    // Wait for client to acknowledge the file size
    ssize_t bytes_message = recv(clientStreamingSocket, message, CHUNK_SIZE, 0);
    if (bytes_message < 0) {
        std::cerr << "Error getting data from client." << std::endl;
    }
    std::cout << message << std::endl;
    if (strcmp(message, "OK") != 0) {
        std::cerr << "Client did not acknowledge file size." << std::endl;
        return;
    }
    else{
        std::cout << "Client acknowledged file size." << std::endl;
    }
    
    // Read file in chunks and send to client
    char buffer[CHUNK_SIZE];
    bzero(buffer, CHUNK_SIZE);
    sleep(1);
    std::cout << "Sending file to client..." << std::endl;
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        ssize_t bytesSent = send(clientStreamingSocket, buffer, file.gcount(), 0);
        if (bytesSent < 0) {
            std::cerr << "Error sending data to client." << std::endl;
            break;
        }
    }

    // Close the file and notify the client of completion
    size.clear();
    file.close();
    std::cout << "File " << filename << " sent successfully." << std::endl;
}

void streamingClientHandler(int clientStreamingSocket){
    char message[CHUNK_SIZE];
    bzero(message, CHUNK_SIZE);
    fileLock.lock();
    sendMP3FileToClient(clientStreamingSocket, streamedFile);
    fileLock.unlock();
    momentLock.lock();
    std::string trackM = std::to_string(trackMoment);
    momentLock.unlock();
    ssize_t bytes_received = recv(clientStreamingSocket, message, 256, 0);
    if(bytes_received < 0) {
        std::cerr << "Error receiving handshake" << std::endl;
    }
    std::cout << "Received: " << message << std::endl;
    ssize_t bytes_sent = send(clientStreamingSocket, trackM.c_str(), trackM.length(), 0);
    if(bytes_sent < 0) {
        std::cerr << "Error sending track moment" << std::endl;
    }
    std::cout << "Track moment sent: " << bytes_sent << std::endl;

}


// Function to handle multiple clients and stream data
void handleClient(int clientSocket, int clientStreamSocket) {
    while(true){
        char buffer[CHUNK_SIZE] ;
        char message[CHUNK_SIZE] ;
        bzero(buffer, CHUNK_SIZE);
        bzero(message, CHUNK_SIZE);
        // Receive commands from the client
        ssize_t bytes_received = recv(clientSocket, buffer, CHUNK_SIZE, 0);
        if(bytes_received < 0) {
            std::cerr << "Error receiving data from client." << std::endl;
            return;
        }
        if(bytes_received == 0) {
            break;
        }
        std::cout << "Received: " << buffer << std::endl;

        if(strcmp(buffer, "HELLO") == 0) {
            
            std::cout << "Client says hello" << std::endl;
            ssize_t bytes_sent = send(clientSocket, "HI", 2, 0);
            if(bytes_sent < 0) {
                std::cerr << "Handshake gone wrong" << std::endl;
            }
            ssize_t bytes_message = recv(clientSocket, message, CHUNK_SIZE, 0);
            if (bytes_message < 0) {
                std::cerr << "Error getting data from client." << std::endl;
            }
        }
        //File transfer from client to server
        if(strcmp(buffer, "FILE") == 0) {
            std::string file_name;
            file_name.clear();
            file_name = get_name(clientSocket);

            if(file_name.empty() || file_name == "invalid") {
                std::cerr << "Invalid file name received." << std::endl;
                continue;
            }

            std::cout << "Client wants to send file: " << file_name << std::endl;
            
            ssize_t bytes_sent = send(clientSocket, "OK", 3, 0);
            if(bytes_sent < 0) {
                std::cerr << "Handshake gone wrong" << std::endl;
            }
            std::cout << "Handshake sent" << std::endl;
            get_file(clientSocket,file_name);
        }
        //Streaming from server to client
        if(strcmp(buffer, "STREAM") == 0) {
            std::cout << "Client wants to stream" << std::endl;
            std::thread(streamingClientHandler, clientStreamSocket).detach();
            
        }

        if(strcmp(buffer, "END") == 0) {
            break;
        }
    }
    close(clientSocket);
    close(clientStreamSocket);
}

  
int main() {
    signal(SIGINT, signalHandler);
    signal(SIGPIPE, SIG_IGN);

    serverControlSocket = socket(AF_INET, SOCK_STREAM, 0);
    serverStreamSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverControlSocket < 0) {
        std::cerr << "Error creating control socket." << std::endl;
        return 1;
    }

    if (serverStreamSocket < 0) {
        std::cerr << "Error creating stream socket." << std::endl;
        return 1;
    }

    sockaddr_in serverControlAddr, serverStreamAddr;
    serverControlAddr.sin_family = AF_INET;
    serverControlAddr.sin_addr.s_addr = INADDR_ANY;
    serverControlAddr.sin_port = htons(CONTROL_PORT);

    serverStreamAddr.sin_family = AF_INET;
    serverStreamAddr.sin_addr.s_addr = INADDR_ANY;
    serverStreamAddr.sin_port = htons(STREAM_PORT);
    std::cout << "ELO" << std::endl;
    if (bind(serverControlSocket, (struct sockaddr*)&serverControlAddr, sizeof(serverControlAddr)) < 0) {
        std::cerr << "Bind failed." << std::endl;
        close(serverControlSocket);
        return 1;
    }
    if (bind(serverStreamSocket, (struct sockaddr*)&serverStreamAddr, sizeof(serverStreamAddr)) < 0) {
        std::cerr << "Bind failed." << std::endl;
        close(serverControlSocket);
        close(serverStreamSocket);
        return 1;
    }
    if (listen(serverControlSocket, 5) < 0) {
        std::cerr << "Listen failed." << std::endl;
        close(serverStreamSocket);
        close(serverControlSocket);
        return 1;
    }
    if (listen(serverStreamSocket, 5) < 0) {
        std::cerr << "Listen failed." << std::endl;
        close(serverStreamSocket);
        close(serverControlSocket);
        return 1;
    }

    std::cout << "Server listening on port " << CONTROL_PORT << "..." << std::endl;
    std::cout << "Server streaming port " << STREAM_PORT << "..." << std::endl;
    sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    sockaddr_in clientStrAddr;
    socklen_t clientStrAddrLen = sizeof(clientStrAddr);

    // Load MP3 files into queue
    std::cout << "Loading files to queue..." << std::endl;
    for (auto& file : std::filesystem::directory_iterator("./mp3files")) {
        fileQueue.push_back(file.path());
    }
    sendFileQueue(serverControlSocket);
    std::thread(streamTracks).detach();

    while (running) {
        int clientSocket = accept(serverControlSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            std::cerr << "Failed to accept client." << std::endl;
            continue;
        }
        int clientStreamSocket = accept(serverStreamSocket, (struct sockaddr*)&clientStrAddr, &clientStrAddrLen);
        if (clientStreamSocket < 0) {
            std::cerr << "Failed to accept client on streaming socket." << std::endl;
            continue;
        } 

        std::cout << "Client connected." << std::endl;
        std::cout << "Client IP: " << inet_ntoa(clientAddr.sin_addr) << std::endl;
        std::cout << "Client port: " << ntohs(clientAddr.sin_port) << std::endl;
        std::cout << "Client streaming IP: " << inet_ntoa(clientStrAddr.sin_addr) << std::endl;
        std::cout << "Client streaming port: " << ntohs(clientStrAddr.sin_port) << std::endl;

        // Handle the client in a separate thread
        std::thread(handleClient, clientSocket, clientStreamSocket).detach();
    }

    std::cout << "Server shutting down..." << std::endl;
    return 0;
}

//handles ctrl+c signal
void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nCaught SIGINT (Ctrl+C), shutting down...\n";
        
        // Close all client sockets
        for (int clientSocket : streamingSockets) {
            close(clientSocket);
        }
        // Close the server socket if it's open
        if (serverControlSocket != -1) {
            close(serverControlSocket);
            std::cout << "Server socket closed.\n";
        }
        if (serverStreamSocket != -1) {
            close(serverStreamSocket);
            std::cout << "Server socket closed.\n";
        }
        
        // Set the flag to stop the server loop
        running = false;
    }
}

//saves the file from the client
void save_file(const std::string& file_name, const char* data, size_t size) {
    std::ofstream file(file_name, std::ios::binary);
    file.write(data, size);
    file.close();
}

//gets the file from the client
void get_file(int client_socket, std::string file_name) {
    // Receive the file size
    char size_buffer[CHUNK_SIZE];
    ssize_t size_bytes = recv(client_socket, size_buffer, CHUNK_SIZE, 0);
    if (size_bytes <= 0) {
        std::cerr << "Failed to receive file size." << std::endl;
        return;
    }
    size_buffer[size_bytes] = '\0';  // Null-terminate the string
    size_t file_size = std::stoul(size_buffer);
    send(client_socket, "SIZE_OK", 7, 0);  // Acknowledge file size

    size_t total_bytes_received = 0;
    char buffer[CHUNK_SIZE];
    std::string file_data;
    file_data.clear();

    // Receive the file in chunks until the total size is reached
    while (total_bytes_received < file_size) {
        ssize_t bytes_received = recv(client_socket, buffer, CHUNK_SIZE, 0);
        if (bytes_received <= 0) {
            break;
        }
        file_data.append(buffer, bytes_received);
        total_bytes_received += bytes_received;
    }
    std::cout << "Received file, total bytes: " << total_bytes_received << std::endl;

    if (total_bytes_received != file_size) {
    std::cerr << "File transfer incomplete: expected " << file_size 
              << " bytes but received " << total_bytes_received << " bytes." << std::endl;
    return;
    }

    file_name = "mp3files/" + file_name;
    fileQueue.push_back(file_name);
    save_file(file_name, file_data.c_str(), file_data.size());
}
//gets name of the file from the client
std::string get_name(int client_socket) {
    char buffer[CHUNK_SIZE];
    bzero(buffer, CHUNK_SIZE);
    ssize_t bytes_received = recv(client_socket, buffer, CHUNK_SIZE, 0);
    if(bytes_received < 0) {
        std::cerr << "Error receiving data from client." << std::endl;
        return "invalid";
    }
    return std::string(buffer);
}
//iterates through the queue of files and iterates them by seconds
void streamTracks(){
    std::cout << "Streaming thread started." << std::endl;
    av_register_all();
    while(running) {
        std::cout << "Size of queue:" << fileQueue.size() << std::endl;
        if(fileQueue.size() > 0) {
            std::string filename = fileQueue.front();
            AVFormatContext* formatContext = nullptr;
            if (avformat_open_input(&formatContext, filename.c_str(), nullptr, nullptr) != 0) {
                std::cerr << "Error: Could not open file " << filename << std::endl;
                continue;
            }

            if (avformat_find_stream_info(formatContext, nullptr) < 0) {
                std::cerr << "Error: Could not find stream information." << std::endl;
                avformat_close_input(&formatContext);
                continue;
            }

            int64_t duration = formatContext->duration;
            double durationInSeconds = 0;
            int dur;
            if (duration != AV_NOPTS_VALUE) {
                durationInSeconds = duration / (double)AV_TIME_BASE;
                std::cout << "Duration: " << durationInSeconds << " seconds" << std::endl;
            } else {
                std::cerr << "Error: Could not determine duration." << std::endl;
                continue;
            }
            dur = int(durationInSeconds);
            std::cout << "Duration: " << dur << " seconds" << std::endl;
            fileLock.lock();
            streamedFile = filename;
            fileLock.unlock();
            for(int i = 0; i < dur; i++){
                momentLock.lock();
                trackMoment = i;
                std::cout << "Track: " << filename << " Time: " << trackMoment << std::endl;
                momentLock.unlock();
                sleep(1);
            }
            fileQueue.erase(fileQueue.begin());
        }
        else{
            std::cout << "No files in queue." << std::endl;
            sleep(3);
        }
        
    }
}