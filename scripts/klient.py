import socket
import time
import threading
from pydub import AudioSegment
from pydub.playback import play
import pygame
import simpleaudio as sa
import io
import os

SERVER_HOST = '192.168.0.11'
SERVER_PORT = 1100
SERVER_STREAMING_PORT = 1101
SERVER_QUEUE_PORT = 1102
CHUNK_SIZE = 4096
MESSAGE_SIZE = 256
file_queue = []
stop_playing = threading.Event()
streaming = False
playing = False
working = True

def receive_file_queue(client_socket):
    try:
        # Receive the file queue as a single string
        data = client_socket.recv(CHUNK_SIZE).decode('utf-8')
        
        # Split the received data by newlines to get individual track names
        track_list = data.splitlines()
        
        # Return the list of tracks
        return track_list
    except UnicodeDecodeError as e:
        print(f"Decoding error: {e}")
        return []  # Return an empty list in case of error



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

#play file in thread
def play_file(filename, start_time):
    global playing
    pygame.mixer.init()
    pygame.mixer.music.load(filename)
    pygame.mixer.music.play(start=start_time)

    while pygame.mixer.music.get_busy():
        if stop_playing.is_set():
            print("Stopping playback...")
            pygame.mixer.music.stop()
            playing = False
            
            break
        time.sleep(1)
    playing = False
#check for streaming in thread
def stream_song(client_socket, client_streaming_socket):
    global working
    global playing
    global streaming
    while(working == True):
        if streaming == True and playing == False:
            client_socket.send('STREAM'.encode('utf-8'))
            audio_Segment = receiveMP3File(client_streaming_socket)
            tracktime = client_streaming_socket.recv(MESSAGE_SIZE)
            tracktime = int(tracktime.decode('utf-8'))
            clear_socket_buffer(client_streaming_socket)
            print(f"Start from {tracktime} seconds")
            stop_playing.clear()
            playing_thread = threading.Thread(target=play_file, args=(audio_Segment.export(format='mp3'), tracktime))
            playing_thread.start()
            playing = True
            del tracktime
            del audio_Segment
        elif streaming == False and playing == True:
            stop_playing.set()
            playing_thread.join()
            playing = False
        else:
            time.sleep(1)
    if playing:
        stop_playing.set()
        playing_thread.join()
        playing = False


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


def update_getter(client_queue_socket):
    global file_queue
    while(working):
        
        update = client_queue_socket.recv(MESSAGE_SIZE).decode('utf-8')
        if(update[:4] == 'SKIP'):
            print('Received skip')
            stop_playing.set()
        else:
            file_queue = update.splitlines()
        time.sleep(2)


if __name__ == '__main__':
    try:
        client_socket = socket.socket()
        client_socket.connect((SERVER_HOST, SERVER_PORT))
        client_streaming_socket = socket.socket()
        client_streaming_socket.connect((SERVER_HOST, SERVER_STREAMING_PORT))
        client_queue_socket = socket.socket()
        client_queue_socket.connect((SERVER_HOST, SERVER_QUEUE_PORT))
        print("Connected to server.")
        streaming_thread = threading.Thread(target=stream_song, args=(client_socket,client_streaming_socket))
        streaming_thread.start()
        queue_thread = threading.Thread(target=update_getter, args=(client_queue_socket,))
        queue_thread.start()
        while(True):
            #get next song from server if streaming is on and song isnt playing
            
            action = input("Action: 'FILE'/'STREAM/'HELLO'/'QUEUE'/'QUEUCHANGE'/'STOP'/'END': ")
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
                streaming = True

            elif action == 'STOP':
                if streaming:
                    streaming = False
                    print("Streaming stopped")
                else:
                    print("Not streaming")
            
            elif action == 'QUEUECHANGE':
                client_socket.send(action.encode('utf-8'))
                handshakeqc = client_socket.recv(MESSAGE_SIZE)
                print(handshakeqc.decode('utf-8'))
                if(handshakeqc.decode('utf-8') == "NIE"):
                    print("Queue is being changed by another user")

                    continue
                queue_change = input("Enter queue action('SWAP'/'DELETE'/'SKIP'): ")
                if(queue_change == 'SWAP'):
                    client_socket.send(queue_change.encode('utf-8'))
                    handshakesw = client_socket.recv(MESSAGE_SIZE)
                    swapIndex1 = input("Enter first track index: ")
                    swapIndex2 = input("Enter second track index: ")
                    indexes = swapIndex1 + ' ' + swapIndex2
                    client_socket.send(indexes.encode('utf-8'))
                    
                elif(queue_change == 'DELETE'):
                    client_socket.send(queue_change.encode('utf-8'))
                    handshakedel = client_socket.recv(MESSAGE_SIZE)
                    fileIndex = input("Enter track index: ")
                    fileIndex = int(fileIndex)
                    print(file_queue[fileIndex])
                    client_socket.send(file_queue[fileIndex].encode('utf-8'))
                    
                elif(queue_change == 'SKIP'):
                    client_socket.send(queue_change.encode('utf-8'))

                

            elif action == "QUEUE":
                print(file_queue)

            elif action == 'HELLO':
                client_socket.send(action.encode('utf-8'))
                handshake = client_socket.recv(MESSAGE_SIZE)
                client_socket.send("Hello".encode('utf-8'))

            elif action == "END":
                client_socket.send(action.encode('utf-8'))
                working = False
                streaming = False
                client_streaming_socket.close()
                client_queue_socket.close()
                queue_thread.end()
                streaming_thread.join()
                queue_thread.join()
                break
            else:
                print("Invalid action.")
    except Exception as KeyboardInterrupt:
        client_socket.close()       
