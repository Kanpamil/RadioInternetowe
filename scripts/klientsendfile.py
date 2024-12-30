import socket

def send_mp3_file(file_path, server_ip, server_port):
    # Create a socket
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        # Connect to the server
        client_socket.connect((server_ip, server_port))

        # Open the MP3 file in binary mode
        with open(file_path, 'rb') as file:
            while chunk := file.read(4096):  # Read 1KB at a time
                client_socket.sendall(chunk)  # Send the chunk to the server

        print("File sent successfully.")

    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        client_socket.close()

# Example usage:
send_mp3_file('../mp3all/test2.mp3', '127.0.0.1', 12345)
