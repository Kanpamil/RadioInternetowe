#include <iostream>
#include <fstream>
#include <queue>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 12345  // Port number
#define MAX_BUF_SIZE 1024  // Max buffer size (1KB)

std::queue<std::string> fileQueue;

void save_file(const std::string& file_name, const char* data, size_t size) {
    std::ofstream file(file_name, std::ios::binary);
    file.write(data, size);
    file.close();
}

void handle_client(int client_socket) {
    char buffer[MAX_BUF_SIZE];
    size_t total_bytes_received = 0;

    // Receive the MP3 file in chunks
    std::string file_data;
    while (true) {
        ssize_t bytes_received = recv(client_socket, buffer, MAX_BUF_SIZE, 0);
        if (bytes_received <= 0) {
            break;
        }
        file_data.append(buffer, bytes_received);
        total_bytes_received += bytes_received;
    }

    // Add the received file to the queue
    fileQueue.push(file_data);
    std::cout << "Received file, total bytes: " << total_bytes_received << std::endl;

    // Optionally save the file to disk
    save_file("mp3files/received_audio.mp3", file_data.c_str(), file_data.size());
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        std::cerr << "Error opening socket." << std::endl;
        return -1;
    }

    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error binding socket." << std::endl;
        return -1;
    }

    // Listen for incoming connections
    listen(server_socket, 5);
    std::cout << "Server listening on port " << PORT << std::endl;

    while (true) {
        // Accept incoming client connection
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            std::cerr << "Error accepting connection." << std::endl;
            continue;
        }

        std::cout << "Client connected." << std::endl;

        // Handle the client
        handle_client(client_socket);

        // Close client socket after processing
        close(client_socket);
        std::cout << "Client disconnected." << std::endl;
    }

    // Close server socket
    close(server_socket);

    return 0;
}
