#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const int PORT = 8080;
const int BUFFER_SIZE = 4096;

// Funkcja wysyłania metadanych
void send_metadata(int client_socket, const std::string& metadata) {
    std::string header = "META" + std::to_string(metadata.size());
    header.resize(8, '0'); // Ustawienie stałej długości nagłówka
    send(client_socket, header.c_str(), header.size(), 0);
    send(client_socket, metadata.c_str(), metadata.size(), 0);
}

// Funkcja strumieniowania danych audio
void stream_audio(int client_socket, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Nie można otworzyć pliku audio.\n";
        return;
    }

    char buffer[BUFFER_SIZE];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        std::string header = "DATA" + std::to_string(file.gcount());
        header.resize(8, '0');
        send(client_socket, header.c_str(), header.size(), 0);
        send(client_socket, buffer, file.gcount(), 0);
    }
    file.close();
    std::cout << "Wysłano cały plik audio.\n";
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Tworzenie gniazda
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Ustawienia socketu
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bindowanie socketu
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Nasłuchiwanie połączeń
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Serwer działa na porcie " << PORT << ". Oczekiwanie na klienta...\n";

    if ((client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Połączono z klientem.\n";


    close(client_socket);
    close(server_fd);
    return 0;
}
