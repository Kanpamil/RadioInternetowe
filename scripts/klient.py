import socket
import time
import threading
from pydub import AudioSegment
from pydub.playback import play
import simpleaudio as sa
import io
import os

SERVER_HOST = 'localhost'
SERVER_PORT = 1100
SERVER_STREAMING_PORT = 1101
CHUNK_SIZE = 4096
MESSAGE_SIZE = 256

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

def play_audio_segment(audio_segment):
    # Convert AudioSegment to raw audio data
    raw_data = audio_segment.raw_data
    play_obj = sa.play_buffer(raw_data, num_channels=audio_segment.channels, 
                              bytes_per_sample=audio_segment.sample_width, 
                              sample_rate=audio_segment.frame_rate)
    play_obj.wait_done()  # Wait until playback finishes

def play_streamed_mp3(client_socket):
    audio_buffer = io.BytesIO()  # buffer to hold the audio data in memory
    total_data_received = 0
    play_thread = None
    try:
        while True:
            # Receive data in chunks
            chunk = client_socket.recv(CHUNK_SIZE)
            if not chunk:
                break

            # Append the chunk to the buffer
            audio_buffer.write(chunk)
            total_data_received += len(chunk)

            # If we have enough data (50 chunks), process for playback
            if total_data_received > CHUNK_SIZE * 50:
                # Prepare the audio segment
                audio_buffer.seek(0)
                audio_segment = AudioSegment.from_file(audio_buffer, format="mp3")
                
                # Ensure no other thread is playing audio
                if play_thread and play_thread.is_alive():
                    play_thread.join()  # Wait for the previous thread to finish
                
                # Start playing audio in a new thread
                play_thread = threading.Thread(target=play_audio_segment, args=(audio_segment,))
                play_thread.start()

                # Reset the buffer after starting playback
                audio_buffer = io.BytesIO()
                total_data_received = 0
    except Exception as e:
        print(f"Error: {e}")

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
                play_streamed_mp3(client_streaming_socket)

            elif action == 'HELLO':
                client_socket.send(action.encode('utf-8'))
                handshake = client_socket.recv(MESSAGE_SIZE)
                client_socket.send("Hello".encode('utf-8'))
            else:
                print("Invalid action.")
    except Exception as KeyboardInterrupt:
        client_socket.close()       
