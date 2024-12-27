import socket
from pydub import AudioSegment
from pydub.playback import play
import io

SERVER_HOST = 'localhost'
SERVER_PORT = 1100
CHUNK_SIZE = 4096

def play_streamed_mp3():
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client_socket.connect((SERVER_HOST, SERVER_PORT))

    audio_buffer = io.BytesIO()
    i = 0
    try:
        while True:
            i += 1
            # Receive data in chunks
            chunk = client_socket.recv(CHUNK_SIZE)
            print(f"got new chunk nr {i}")
            if not chunk:
                break

            # Append the chunk to the buffer
            audio_buffer.write(chunk)

            # Decode and play the buffer when sufficient data is available
            if audio_buffer.tell() > CHUNK_SIZE * 50:  # Buffer enough data
                audio_buffer.seek(0)
                audio_segment = AudioSegment.from_file(audio_buffer, format="mp3")
                play(audio_segment)
                audio_buffer = io.BytesIO()  # Reset buffer
    except Exception as e:
        print(f"Error: {e}")
    finally:
        client_socket.close()

if __name__ == "__main__":
    play_streamed_mp3()
