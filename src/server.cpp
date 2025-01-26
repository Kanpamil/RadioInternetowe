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
#include <fcntl.h>
#include <algorithm>
#include <atomic>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#define CONTROL_PORT 1100
#define STREAM_PORT 1101
#define QUEUE_PORT 1102
#define CHUNK_SIZE 4096
#define LUI long unsigned int

std::mutex fileQueueMutex;
std::mutex momentLock;
std::mutex fileLock;
std::mutex clientMutex;

std::vector<std::string> fileQueue;
std::string streamedFile;
int trackMoment = 0;
int numberOfClients = 0;
std::atomic<int> clientsAcknowledged = 0;
int serverControlSocket = -1;
int serverStreamSocket = -1;
int serverQueueSocket = -1;

bool running = true; // Flag to stop the server loop
bool skip = false; // Flag to skip the current track
bool is_queue_changing = false; // Flag to check if the queue is being changed by a client
bool clientSkip = false; // Flag to send skip message to clients

void signalHandler(int signal);

void save_file(const std::string& file_name, const char* data, size_t size);

void get_file(int client_socket, std::string file_name);

void streamTracks();

void sendMP3FileToClient(int clientStreamingSocket, const std::string& filename);

//function to handle file streaming - just file sending and tracktime
void streamingClientHandler(int clientStreamingSocket){
    char message[CHUNK_SIZE];
    bzero(message, CHUNK_SIZE);
    std::cout<<"Streaming client connected."<<std::endl;
    fileLock.lock();
    sendMP3FileToClient(clientStreamingSocket, streamedFile);
    fileLock.unlock();
    
    ssize_t bytes_received = recv(clientStreamingSocket, message, 256, 0);
    if(bytes_received < 0) {
        std::cerr << "Error receiving handshake" << std::endl;
    }
    std::cout << "Received: " << message << std::endl;

    momentLock.lock();
    std::string trackM = std::to_string(trackMoment);
    std::time_t currentTime = std::time(nullptr);
    momentLock.unlock();

    std::string timeString = std::to_string(currentTime);

    ssize_t bytes_sent = send(clientStreamingSocket, trackM.c_str(), trackM.length(), 0);
    if(bytes_sent < 0) {
        std::cerr << "Error sending track moment" << std::endl;
    }
    std::cout << "Track moment sent: " << bytes_sent << std::endl;
    bytes_received = 0;
    bytes_received = recv(clientStreamingSocket, message, 256, 0);
    if(bytes_received < 0) {
        std::cerr << "Error receiving handshake" << std::endl;
    }
    std::cout << "Received: " << message << std::endl;
    bytes_sent = 0;
    bytes_sent = send(clientStreamingSocket, timeString.c_str(), timeString.length(), 0);
    if(bytes_sent < 0) {
        std::cerr << "Error sending moment of probing track" << std::endl;
    }
    

}


//function handling client commands
void handleClient(int clientSocket, int clientStreamSocket, int clientQueueSocket) {
    while(running){
        char buffer[CHUNK_SIZE] ;//buffer for commands
        char message[CHUNK_SIZE] ;//buffer for messages
        bzero(buffer, CHUNK_SIZE);
        bzero(message, CHUNK_SIZE);

        //receive command
        ssize_t bytes_received = recv(clientSocket, buffer, CHUNK_SIZE, 0);
        if(bytes_received < 0) {
            std::cerr << "Error receiving data from client." << std::endl;
            return;
        }
        if(bytes_received == 0) {
            break;
        }
        std::cout << "Received: " << buffer << std::endl;

        //Hello for testing
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
            ssize_t bytes_sent_file = send(clientSocket, "OK ", 3, 0);
            if(bytes_sent_file < 0) {
                std::cerr << "Handshake in file upload gone wrong" << std::endl;
            }
            //getting file name
            std::string file_name;
            file_name.clear();
            char buffer[CHUNK_SIZE];
            bzero(buffer, CHUNK_SIZE);
            ssize_t bytes_received = recv(clientSocket, buffer, CHUNK_SIZE, 0);
            if(bytes_received < 0) {
                std::cerr << "Error receiving file name from client." << std::endl;
            }
            file_name = std::string(buffer);
            if(file_name.empty() || file_name == "invalid") {
                std::cerr << "Invalid file name received." << std::endl;
                continue;
            }

            std::cout << "Client wants to send file: " << file_name << std::endl;
            //sending OK to client - he can start sending file
            ssize_t bytes_sent = send(clientSocket, "OK ", 3, 0);
            if(bytes_sent < 0) {
                std::cerr << "Handshake gone wrong" << std::endl;
            }
            std::cout << "Handshake sent" << std::endl;
            //getting file from client
            get_file(clientSocket,file_name);
        }
        //Streaming from server to client
        if(strcmp(buffer, "STREAM") == 0) {
            std::cout << "Client wants to stream" << std::endl;
            //create thread for streaming
            std::thread(streamingClientHandler, clientStreamSocket).detach();
            
        }
        //Queue handling
        if(strcmp(buffer, "QUEUECHANGE") == 0) {
            //check if queue is already being changed by other client
            if(is_queue_changing){
                std::cout << "Queue is already changing" << std::endl;
                //tell client that queue is changing
                ssize_t bytes_sent_qc = send(clientSocket, "NIEOK", 3, 0);
                if(bytes_sent_qc < 0) {
                    std::cerr << "Handshake gone wrong" << std::endl;
                }
                continue;
            }
            is_queue_changing = true;
            char change[CHUNK_SIZE];
            bzero(change, CHUNK_SIZE);
            //send handshake
            ssize_t bytes_sent_qc = send(clientSocket, "OK", 3, 0);
            if(bytes_sent_qc < 0) {
                std::cerr << "Handshake gone wrong" << std::endl;
            }
            //receive type of change
            ssize_t bytes_received_qc = recv(clientSocket, change, CHUNK_SIZE, 0);
            if(bytes_received_qc < 0) {
                std::cerr << "Error receiving data from client." << std::endl;
            }
            std::cout << "Received: " << change << std::endl;

            //skip current song
            if(strcmp(change,"SKIP") == 0) {
                std::cout << "Client wants to skip file" << std::endl;
                fileLock.lock();
                fileQueue.erase(fileQueue.begin());
                skip = true;
                fileLock.unlock();
                is_queue_changing = false;
            }
            //delete file from queue
            else if(strcmp(change,"DELETE") == 0) {
                std::cout << "Client wants to delete file" << std::endl;
                char file_del[CHUNK_SIZE];
                bzero(file_del, CHUNK_SIZE);
                ssize_t bytes_sent_del = send(clientSocket, "OK", 3, 0);
                if(bytes_sent_del < 0) {
                    std::cerr << "Handshake gone wrong" << std::endl;
                }
                //receive the name of file to delete
                ssize_t bytes_received_del = recv(clientSocket, file_del, CHUNK_SIZE, 0);
                if(bytes_received_del < 0) {
                    std::cerr << "Error receiving data from client." << std::endl;
                }

                std::cout << "Client wants to delete file: " << file_del << std::endl;
                std::string file_name = file_del;
                file_name = "./mp3files/" + file_name;
                fileLock.lock();
                std::cout << "File to delete: " << file_name << std::endl;
                std::vector<std::string>::iterator position = std::find(fileQueue.begin(), fileQueue.end(), file_name);
                if (position != fileQueue.end()){
                    if(*position == streamedFile){
                        skip = true;
                    }
                    fileQueue.erase(position);
                }
                else{
                    std::cerr << "File not found in queue" << std::endl;
                }
                fileLock.unlock();
                is_queue_changing = false;
            }
            //swap files in queue
            else if(strcmp(change,"SWAP") == 0) {
                std::cout << "Client wants to swap files" << std::endl;
                char idx_swap[CHUNK_SIZE];
                bzero(idx_swap, CHUNK_SIZE);
                //send handshake
                ssize_t bytes_sent_swap = send(clientSocket, "OK", 3, 0);
                if(bytes_sent_swap < 0) {
                    std::cerr << "Handshake gone wrong" << std::endl;
                }
                //receive indices of files to swap
                ssize_t bytes_received_swap = recv(clientSocket, idx_swap, CHUNK_SIZE, 0);
                if(bytes_received_swap < 0) {
                    std::cerr << "Error receiving data from client." << std::endl;
                }
                std::string idx_swap_str = idx_swap;
                std::istringstream iss(idx_swap_str);//treat the string as stream to get indices
                int idx1, idx2;
                iss >> idx1 >> idx2;
                std::cout << "Received indices: " << idx1 << ", " << idx2 << std::endl;
                //check if indices are valid and swap filenames in queue
                fileQueueMutex.lock();
                if (idx1 >= 0 && idx1 < int(fileQueue.size()) && idx2 >= 0 && idx2 < int(fileQueue.size())) {
                    std::swap(fileQueue[idx1], fileQueue[idx2]);
                    if(fileQueue[idx1] == streamedFile || fileQueue[idx2] == streamedFile){
                        skip = true;
                    }
                    std::cout << "Swapped files" << fileQueue[idx1] << " and " << fileQueue[idx2] << std::endl;
                    std::cout << "Files swapped successfully!" << std::endl;
                } else {
                    std::cerr << "Invalid indices for swap!" << std::endl;
                }
                is_queue_changing = false;
                fileQueueMutex.unlock();
            }
            else{
                std::cerr << "Invalid command" << std::endl;
                is_queue_changing = false;
            }
        }
        //break the loop if client sends END
        if(strcmp(buffer, "END") == 0) {
            break;
        }
    }
    close(clientSocket);
    close(clientStreamSocket);
    close(clientQueueSocket);
    clientMutex.lock();
    numberOfClients--;
    clientMutex.unlock();
}
void update_sender(int clientQueueSocket){
    bool alreadyAcknowledged = false;
    while(running){
        //if the clients have to receive skip message
        if(clientSkip){
            ssize_t bytes_sent = send(clientQueueSocket, "SKIP", 6, 0);
            if(bytes_sent < 0) {
                std::cerr << "Error sending skip message" << std::endl;
                break;
            }
            else{
                if(alreadyAcknowledged == false){
                    clientsAcknowledged++;
                    std::cout << "Skip message sent." << std::endl;
                }
                
            }

            clientMutex.lock();
            if(clientsAcknowledged == numberOfClients){
                clientSkip = false;
                clientsAcknowledged = 0;
            }
            clientMutex.unlock();

            sleep(2);
        }
        
        //otherwise send queue continously
        else{
            alreadyAcknowledged = false;
            std::string queue = "";
            fileQueueMutex.lock();
            for(LUI i = 0; i < fileQueue.size(); i++) {
                std::string name = fileQueue[i];
                name = name.substr(name.find_last_of("/") + 1);
                queue += name + "\n";
            }
            fileQueueMutex.unlock();
            ssize_t bytes_sent = send(clientQueueSocket, queue.c_str(), queue.length(), 0);
            if(bytes_sent < 0) {
                std::cerr << "Error sending file queue" << std::endl;
                break;
            }
            sleep(2);
        }
        
    }
    close(clientQueueSocket);
 }
  
int main() {
    signal(SIGINT, signalHandler);
    signal(SIGPIPE, SIG_IGN);

    serverControlSocket = socket(AF_INET, SOCK_STREAM, 0);
    serverStreamSocket = socket(AF_INET, SOCK_STREAM, 0);
    serverQueueSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverControlSocket < 0) {
        std::cerr << "Error creating control socket." << std::endl;
        return 1;
    }

    if (serverStreamSocket < 0) {
        std::cerr << "Error creating stream socket." << std::endl;
        return 1;
    }

    if (serverQueueSocket < 0) {
        std::cerr << "Error creating queue socket." << std::endl;
        return 1;
    }

    sockaddr_in serverControlAddr, serverStreamAddr, serverQueueAddr;
    serverControlAddr.sin_family = AF_INET;
    serverControlAddr.sin_addr.s_addr = INADDR_ANY;
    serverControlAddr.sin_port = htons(CONTROL_PORT);

    serverStreamAddr.sin_family = AF_INET;
    serverStreamAddr.sin_addr.s_addr = INADDR_ANY;
    serverStreamAddr.sin_port = htons(STREAM_PORT);

    serverQueueAddr.sin_family = AF_INET;
    serverQueueAddr.sin_addr.s_addr = INADDR_ANY;
    serverQueueAddr.sin_port = htons(QUEUE_PORT);
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
    if (bind(serverQueueSocket, (struct sockaddr*)&serverQueueAddr, sizeof(serverQueueAddr)) < 0) {
        std::cerr << "Bind failed." << std::endl;
        close(serverControlSocket);
        close(serverStreamSocket);
        close(serverQueueSocket);
        return 1;
    }
    if (listen(serverControlSocket, 5) < 0) {
        std::cerr << "Listen failed." << std::endl;
        close(serverStreamSocket);
        close(serverControlSocket);
        close(serverQueueSocket);
        return 1;
    }
    if (listen(serverStreamSocket, 5) < 0) {
        std::cerr << "Listen failed." << std::endl;
        close(serverStreamSocket);
        close(serverControlSocket);
        close(serverQueueSocket);
        return 1;
    }
    if (listen(serverQueueSocket, 5) < 0) {
        std::cerr << "Listen failed." << std::endl;
        close(serverStreamSocket);
        close(serverControlSocket);
        close(serverQueueSocket);
        return 1;
    }

    std::cout << "Server listening on port " << CONTROL_PORT << "..." << std::endl;
    std::cout << "Server streaming port " << STREAM_PORT << "..." << std::endl;
    std::cout << "Server queue port " << QUEUE_PORT << "..." << std::endl;
    sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    sockaddr_in clientStrAddr;
    socklen_t clientStrAddrLen = sizeof(clientStrAddr);
    sockaddr_in clientQueueAddr;
    socklen_t clientQueueAddrLen = sizeof(clientQueueAddr);

    // Load MP3 files into queue
    std::cout << "Loading files to queue..." << std::endl;
    for (auto& file : std::filesystem::directory_iterator("./mp3files")) {
        fileQueue.push_back(file.path());
    }
    //iteracja przez pliki w osobnym wątku
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

        int clientQueueSocket = accept(serverQueueSocket, (struct sockaddr*)&clientQueueAddr, &clientQueueAddrLen);
        if (clientQueueSocket < 0) {
            std::cerr << "Failed to accept client on queue socket." << std::endl;
            continue;
        }

        clientMutex.lock();
        numberOfClients++;
        std::cout << "Number of clients: " << numberOfClients << std::endl;
        clientMutex.unlock();
        std::cout << "Client connected." << std::endl;
        std::cout << "Client IP: " << inet_ntoa(clientAddr.sin_addr) << std::endl;
        std::cout << "Client port: " << ntohs(clientAddr.sin_port) << std::endl;
        std::cout << "Client streaming IP: " << inet_ntoa(clientStrAddr.sin_addr) << std::endl;
        std::cout << "Client streaming port: " << ntohs(clientStrAddr.sin_port) << std::endl;

        // Handle the client in a separate thread
        std::thread(handleClient, clientSocket, clientStreamSocket,clientQueueSocket).detach();
        std::thread(update_sender, clientQueueSocket).detach();
    }

    std::cout << "Server shutting down..." << std::endl;
    std::cout << "Waiting 3s for threads to finish..." << std::endl;
    sleep(3);
    std::cout << "Server shutdown complete." << std::endl;
    return 0;
}

//handles ctrl+c signal
void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nCaught SIGINT (Ctrl+C), shutting down...\n";
        
        // Close the server socket if it's open
        if (serverControlSocket != -1) {
            close(serverControlSocket);
            std::cout << "Server socket closed.\n";
        }
        if (serverStreamSocket != -1) {
            close(serverStreamSocket);
            std::cout << "Server socket closed.\n";
        }
        if(serverQueueSocket != -1){
            close(serverQueueSocket);
            std::cout << "Server socket closed.\n";
        }
        
        // Set the flag to stop the server loop
        running = false;
    }
}

//function sending MP3 file to client used in streaming
void sendMP3FileToClient(int clientStreamingSocket, const std::string& filename) {
    char message[CHUNK_SIZE]; // Buffer for messages
    bzero(message,CHUNK_SIZE);
    std::ifstream file(filename, std::ios::binary);//open file in binary since it is audio file
    
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }
    //get filesize
    file.seekg(0, std::ios::end);  
    std::streamsize fileSize = 0;
    fileSize = file.tellg();  
    file.seekg(0, std::ios::beg);
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

    //check if client acknowledged the file size
    if (strcmp(message, "OK") != 0) {
        std::cerr << "Client did not acknowledge file size." << std::endl;
        return;
    }
    else{
        std::cout << "Client acknowledged file size." << std::endl;
    }
    
    // send the file to the client
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

    // close the file and clear the size buffer
    file.close();
    std::cout << "File " << filename << " sent successfully." << std::endl;
}

//saves the file from the client
void save_file(const std::string& file_name, const char* data, size_t size) {
    std::ofstream file(file_name, std::ios::binary);// binary mode since its audio file
    file.write(data, size);
    file.close();
}

//gets the file from the client and saves it in dedicated directory
void get_file(int client_socket, std::string file_name) {
    // receive the file size
    char size_buffer[CHUNK_SIZE];
    bzero(size_buffer, CHUNK_SIZE);
    ssize_t size_bytes = recv(client_socket, size_buffer, CHUNK_SIZE, 0);
    if (size_bytes <= 0) {
        std::cerr << "Failed to receive file size." << std::endl;
        return;
    }
    //convert it to size_t
    std::string size_str(size_buffer, size_bytes);
    size_t file_size = std::stoul(size_str);

    send(client_socket, "SIZE_OK", 7, 0);  // Acknowledge file size

    size_t total_bytes_received = 0;
    char buffer[CHUNK_SIZE];
    std::string file_data;
    file_data.clear();

    // receive the file data loop
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
        std::cerr << "File transfer incomplete" << file_size << " bytes wanted but received " << total_bytes_received << " bytes." << std::endl;
        return;
    }
    //save the file in dedicated directory
    file_name = "mp3files/" + file_name;
    fileQueue.push_back(file_name);
    save_file(file_name, file_data.c_str(), file_data.size());
}


//iterates through the queue of files and iterates them by seconds
void streamTracks(){
    std::cout << "Streaming thread started." << std::endl;
    while(running) {
        std::cout << "Size of queue:" << fileQueue.size() << std::endl;
        //if there are files in queue
        if(fileQueue.size() > 0) {
            //segment for getting the duration of the file
            std::string filename = fileQueue.front();
            //set the file to be streamed so the streaming thread can send it
            fileLock.lock();
            streamedFile = filename;
            fileLock.unlock();
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
            } else {
                std::cerr << "Error: Could not determine duration." << std::endl;
                continue;
            }
            dur = int(durationInSeconds);
            std::cout << "Duration: " << dur << " seconds" << std::endl;
            std::cout << "Streaming file: " << filename << std::endl;
            sleep(5);//wait 5s because of the clients delay
            //iterate through all seconds of the file
            for(int i = 0; i < dur; i++){
                //if the file needs to be skipped stop iterating
                if(skip){
                    clientSkip = true;
                    break;
                }
                momentLock.lock();
                trackMoment = i;
                if(trackMoment%10 == 0){
                    std::cout << "Track: " << filename << " Time: " << trackMoment << std::endl;
                }
                momentLock.unlock();
                sleep(1);
            }
            //if the file ended on its own erase it - when the skip flag is set it is erased in the client command
            if(!skip){
                fileQueue.erase(fileQueue.begin());
            }
            skip = false;


            std::cout << "Track: " << filename << " finished." << std::endl;
            momentLock.lock();
            trackMoment = 0;
            momentLock.unlock();
        }
        else{//if all the songs were streamed load them again
            std::cout << "No files in queue." << std::endl;
            std::cout << "Loading files to queue again" << std::endl;
            for (auto& file : std::filesystem::directory_iterator("./mp3files")) {
                fileQueue.push_back(file.path());
            }
        }
        
    }
}