import socket
import threading
from pydub import AudioSegment
from pydub.playback import play
import simpleaudio as sa
import io

SERVER_HOST = 'localhost'
SERVER_PORT = 1100
SERVER_STREAMING_PORT = 1101
CHUNK_SIZE = 4096

def receive_file_queue(client_socket):
    print("Receiving file queue...")
    try:
        data = client_socket.recv(2048).decode('utf-8')
    except UnicodeDecodeError as e:
        print(f"Decoding error: {e}")
        data = None
    return data


def receive_metadata(client_socket):
    print("Receiving metadata...")
    try:
        data = client_socket.recv(2048).decode('utf-8')
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

if __name__ == '__main__':
    client_socket = socket.socket()
    client_socket.connect((SERVER_HOST, SERVER_PORT))
    client_streaming_socket = socket.socket()
    client_streaming_socket.connect((SERVER_HOST, SERVER_STREAMING_PORT))
    
    client_socket.send("HELLO".encode('utf-8'))
    message = client_streaming_socket.recv(2048)
    print(message.decode('utf-8'))
    
    client_socket.close()
