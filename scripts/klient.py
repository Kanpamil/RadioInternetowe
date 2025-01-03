import socket
import time
import threading
from pydub import AudioSegment
from pydub.playback import play
import pygame
import simpleaudio as sa
import io
import os

SERVER_HOST = 'localhost'
SERVER_PORT = 1100
SERVER_STREAMING_PORT = 1101
CHUNK_SIZE = 4096
MESSAGE_SIZE = 256

stop_streaming = threading.Event()

def receive_file_queue(client_socket):
    print("Receiving file queue...")
    try:
        data = client_socket.recv(CHUNK_SIZE).decode('utf-8')
    except UnicodeDecodeError as e:
        print(f"Decoding error: {e}")
        data = None
    return data


def receive_metadata(client_socket):
    print("Receiving metadata...")
    try:
        data = client_socket.recv(CHUNK_SIZE).decode('utf-8')
    except UnicodeDecodeError as e:
        print(f"Decoding error: {e}")
        data = None
    return data

def clear_socket_buffer(client_socket):
    """Clear any leftover data in the socket buffer."""
    try:
        client_socket.settimeout(1)  # Set a timeout of 1 second (adjust as necessary)
        while True:
            data = client_socket.recv(1024)  # Try to read data from the socket
            if not data:
                break  # Break when no more data is available
    except socket.timeout:
        pass  # Timeout reached, no more data to read
    finally:
        client_socket.settimeout(None)  # Reset the timeout back to blocking mode

def receiveMP3File(client_streaming_socket):
    try:
        print("elo")
        audio_buffer = io.BytesIO()  # Create an in-memory buffer

        file_size = client_streaming_socket.recv(MESSAGE_SIZE)# Get the file size
        print(file_size)
        file_size = file_size.decode('utf-8')
        print(file_size)
        file_size = int(file_size)
        print(f"Receiving MP3 file of size {file_size} bytes...")
        client_streaming_socket.send("OK".encode('utf-8'))
        print("sent acknowledgment after filesize")  # Acknowledge the file size
        received_size = 0
        while received_size < file_size:
            if received_size == 0:
                print("Receiving first chunk")
            # Receive data in chunks
            chunk = client_streaming_socket.recv(CHUNK_SIZE)
            if not chunk:
                break

            # Write the chunk to the buffer
            audio_buffer.write(chunk)
            received_size += len(chunk)

        # Reset the buffer and convert to an audio segment
        client_streaming_socket.send("OK".encode('utf-8'))  # Acknowledge the file size
        print("sent acknowledgment after receiving file")
        audio_buffer.seek(0)
        audio_segment = AudioSegment.from_file(audio_buffer, format="mp3")
        del audio_buffer  # Delete the buffer to free up memory
        del file_size
        del received_size
        return audio_segment

    except Exception as e:
        print(f"Error receiving MP3 file: {e}")


def play_file(filename, start_time):
    pygame.mixer.init()
    pygame.mixer.music.load(filename)
    pygame.mixer.music.play(start=start_time)

    while pygame.mixer.music.get_busy():
        if stop_streaming.is_set():
            print("Stopping playback...")
            pygame.mixer.music.stop()
            break
        time.sleep(1)


def send_mp3_file(file_path, client_socket):
    try:
        file_size = os.path.getsize(file_path)  # Get the size of the file
        client_socket.send(str(file_size).encode('utf-8'))  # Send the file size
        client_socket.recv(MESSAGE_SIZE)  # Wait for the server to acknowledge

        with open(file_path, 'rb') as file:
            while chunk := file.read(CHUNK_SIZE):  # Read file in chunks
                client_socket.sendall(chunk)  # Send each chunk

        print("File sent successfully.")

    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == '__main__':
    try:
        streaming = False
        client_socket = socket.socket()
        client_socket.connect((SERVER_HOST, SERVER_PORT))
        client_streaming_socket = socket.socket()
        client_streaming_socket.connect((SERVER_HOST, SERVER_STREAMING_PORT))
        while(True):
            action = input("Action: 'FILE'/'STREAM/'HELLO'")

            if action == 'FILE':
                client_socket.send(action.encode('utf-8'))
                fileName = input("Enter file name: ")
                print(fileName)
                client_socket.send(fileName.encode('utf-8'))
                handshake = client_socket.recv(MESSAGE_SIZE)
                print(handshake.decode('utf-8'))
                send_mp3_file('../mp3all/'+fileName, client_socket)

            elif action == 'STREAM':
                if streaming:
                    print("Already streaming")
                    continue
                client_socket.send(action.encode('utf-8'))
                audio_Segment = receiveMP3File(client_streaming_socket)
                tracktime = client_streaming_socket.recv(MESSAGE_SIZE)
                tracktime = int(tracktime.decode('utf-8'))
                clear_socket_buffer(client_streaming_socket)
                print(f"Start from {tracktime} seconds")
                stop_streaming.clear()
                streaming_thread = threading.Thread(target=play_file, args=(audio_Segment.export(format='mp3'), tracktime))
                streaming_thread.start()
                streaming = True
                del tracktime
                del audio_Segment
            elif action == 'STOP':
                if streaming:
                    stop_streaming.set()
                    streaming_thread.join()
                    streaming = False
                    print("Streaming stopped")
                else:
                    print("Not streaming")
                

            elif action == 'HELLO':
                client_socket.send(action.encode('utf-8'))
                handshake = client_socket.recv(MESSAGE_SIZE)
                client_socket.send("Hello".encode('utf-8'))

            elif action == "END":
                client_socket.send(action.encode('utf-8'))
                break
            else:
                print("Invalid action.")
    except Exception as KeyboardInterrupt:
        client_socket.close()       
