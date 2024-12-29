import socket
import pygame
import io
import time
import tempfile
import os
import threading

# Configuration
SERVER_IP = "127.0.0.1"  # The IP of your server
PORT = 1100  # Port where server is listening
CHUNK_SIZE = 627  # Size of data chunks to receive
# Initialize pygame mixer for audio playback
pygame.mixer.init()

def play_audio(temp_filename):
    pygame.mixer.music.load(temp_filename)
    pygame.mixer.music.play()
    


# Function to connect to the server and receive the data
def receive_and_play():
    # Create a socket object
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    # Connect to the server
    client_socket.connect((SERVER_IP, PORT))
    print(f"Connected to server at {SERVER_IP}:{PORT}")

    # Buffer to hold the received audio data
    audio_buffer = io.BytesIO()
    play_thread = None
    i =0
    try:
        while True:
            i += 1
            print(f"Receiving data chunk {i}")
            # Receive data in chunks
            data = client_socket.recv(CHUNK_SIZE)
            if not data:
                print("No more data received. Closing connection.")
                break
            # Write the data to the audio buffer
            audio_buffer.write(data)

            # If we have enough data in the buffer to play (can adjust as needed)
            if audio_buffer.tell() > CHUNK_SIZE * 90:
                audio_buffer.seek(0)

                # Create a temporary file to save the buffer data
                with tempfile.NamedTemporaryFile(delete=False, suffix=".mp3") as temp_file:
                    temp_file.write(audio_buffer.getvalue())
                    temp_filename = temp_file.name

                # Play the temporary file
                
                # Wait for the previous thread to finish
                if play_thread and play_thread.is_alive():
                    play_thread.join()
                play_thread = threading.Thread(target=play_audio,args=(temp_filename,))
                play_thread.start()

                # Remove the temporary file after playing
                os.remove(temp_filename)
                
                # Reset the buffer for the next chunk
                audio_buffer.seek(0)
                audio_buffer.truncate(0)

    except Exception as e:
        print(f"Error: {e}")
    finally:
        client_socket.close()
        print("Connection closed.")

if __name__ == "__main__":
    receive_and_play()
