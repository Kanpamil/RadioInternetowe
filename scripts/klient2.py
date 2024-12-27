import socket
import threading
from pydub import AudioSegment
from pydub.playback import play
import io

SERVER_HOST = 'localhost'
SERVER_PORT = 1100
CHUNK_SIZE = 2048

def play_audio_segment(audio_segment):
    """Function to play the audio in a separate thread."""
    play(audio_segment)

def play_streamed_mp3():
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client_socket.connect((SERVER_HOST, SERVER_PORT))

    audio_buffer = io.BytesIO()  # buffer to hold the audio data in memory
    play_thread = None  # Store reference to the current playback thread
    i = 0
    try:
        while True:
            i += 1
            print(i)
            # Receive data in chunks
            chunk = client_socket.recv(CHUNK_SIZE)
            if not chunk:
                break

            # Append the chunk to the buffer
            audio_buffer.write(chunk)

            # If we have enough data (50 chunks), we start playing it
            if audio_buffer.tell() > CHUNK_SIZE * 50:
                # The audio data is now ready for playback
                audio_buffer.seek(0)
                audio_segment = AudioSegment.from_file(audio_buffer, format="mp3")

                # If a play thread is already running, wait for it to finish
                if play_thread and play_thread.is_alive():
                    play_thread.join()

                # Start a new thread to play the audio segment
                play_thread = threading.Thread(target=play_audio_segment, args=(audio_segment,))
                play_thread.start()

                # Reset the buffer after starting playback
                audio_buffer = io.BytesIO()

    except Exception as e:
        print(f"Error: {e}")
    finally:
        client_socket.close()

        # Ensure the last play thread finishes before closing
        if play_thread and play_thread.is_alive():
            play_thread.join()

if __name__ == "__main__":
    play_streamed_mp3()
