import socket
import pygame
import io
from pydub import AudioSegment
import numpy as np

SERVER_HOST = 'localhost'
SERVER_PORT = 1100
CHUNK_SIZE = 2048

def play_audio_segment(audio_data):
    """Function to play audio data using pygame.mixer."""
    # Initialize pygame mixer if not already initialized
    if not pygame.mixer.get_init():
        pygame.mixer.init(frequency=44100, size=-16, channels=2)

    # Convert raw audio data (PCM format) to a Sound object
    sound = pygame.mixer.Sound(audio_data)
    sound.play()

def play_streamed_mp3():
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client_socket.connect((SERVER_HOST, SERVER_PORT))

    audio_buffer = io.BytesIO()  # buffer to hold the audio data in memory
    play_thread = None  # Store reference to the current playback thread
    i = 0
    try:
        while True:
            print(i)
            i += 1
            # Receive data in chunks
            chunk = client_socket.recv(CHUNK_SIZE)
            if not chunk:
                break

            # Append the chunk to the buffer
            audio_buffer.write(chunk)

            # If we have enough data (50 chunks), we start playing it
            if audio_buffer.tell() > CHUNK_SIZE * 25:
                # The audio data is now ready for playback
                audio_buffer.seek(0)
                
                # Decode the MP3 data using pydub into raw PCM data
                audio_segment = AudioSegment.from_file(audio_buffer, format="mp3")
                
                # Convert the audio to raw PCM format (16-bit mono/stereo at 44.1 kHz)
                raw_audio = audio_segment.raw_data

                # Play audio using pygame
                play_audio_segment(raw_audio)

                # Reset the buffer after starting playback
                audio_buffer = io.BytesIO()

    except Exception as e:
        print(f"Error: {e}")
    finally:
        client_socket.close()

if __name__ == "__main__":
    pygame.mixer.init(frequency=44100, size=-16, channels=2)  # Initialize pygame mixer globally
    play_streamed_mp3()
