import socket
import threading
import tkinter as tk
from tkinter import filedialog, messagebox
import tkinter.simpledialog as simpledialog
import time
from pydub import AudioSegment
from pydub.playback import play
import pygame
import simpleaudio as sa
import io
import os

# Server configuration
SERVER_HOST = 'localhost'
SERVER_PORT = 1100
SERVER_STREAMING_PORT = 1101
SERVER_QUEUE_PORT = 1102
CHUNK_SIZE = 4096
MESSAGE_SIZE = 256

# Global variables
file_queue = []
stop_playing = threading.Event()
streaming = False
playing = False
working = True
current_song = ""
action = None  # To store the current action selected by a button

def set_action(new_action):
    global action
    if action is None:  # Only change if action is None
        action = new_action
    else:
        messagebox.showinfo("Action Pending", "Please wait for the current action to complete.")
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

def handle_queue_change(client_socket):
    global file_queue

    def perform_swap():
        swap_index1 = simpledialog.askinteger("Swap", "Enter first track index:")
        swap_index2 = simpledialog.askinteger("Swap", "Enter second track index:")

        if swap_index1 is not None and swap_index2 is not None:
            client_socket.send("SWAP".encode('utf-8'))
            client_socket.recv(MESSAGE_SIZE)  # Handshake
            indexes = f"{swap_index1} {swap_index2}"
            client_socket.send(indexes.encode('utf-8'))
            messagebox.showinfo("Success", "Tracks swapped successfully!")
        close_queue_change_buttons()

    def perform_delete():
        file_index = simpledialog.askinteger("Delete", "Enter track index:")

        if file_index is not None and 0 <= file_index < len(file_queue):
            client_socket.send("DELETE".encode('utf-8'))
            client_socket.recv(MESSAGE_SIZE)  # Handshake
            client_socket.send(file_queue[file_index].encode('utf-8'))
            messagebox.showinfo("Success", f"Track {file_queue[file_index]} deleted successfully!")
        close_queue_change_buttons()

    def perform_skip():
        client_socket.send("SKIP".encode('utf-8'))
        messagebox.showinfo("Success", "Track skipped!")
        close_queue_change_buttons()

    def close_queue_change_buttons():
        for widget in queue_change_frame.winfo_children():
            widget.destroy()
        queue_change_frame.pack_forget()

    # Request queue change from the server
    client_socket.send("QUEUECHANGE".encode('utf-8'))
    handshakeqc = client_socket.recv(MESSAGE_SIZE).decode('utf-8')

    if handshakeqc == "NIE":
        messagebox.showerror("Error", "Queue is being changed by another user")
        return

    # Create buttons for SWAP, DELETE, and SKIP
    queue_change_frame = tk.Frame(root)
    queue_change_frame.pack(pady=10)

    tk.Label(queue_change_frame, text="Select a queue action:").pack(pady=5)
    tk.Button(queue_change_frame, text="Swap Tracks", command=perform_swap).pack(pady=5)
    tk.Button(queue_change_frame, text="Delete Track", command=perform_delete).pack(pady=5)
    tk.Button(queue_change_frame, text="Skip Track", command=perform_skip).pack(pady=5)
    tk.Button(queue_change_frame, text="Cancel", command=close_queue_change_buttons).pack(pady=10)

def handle_action(client_socket, client_streaming_socket, client_queue_socket):
    global action, streaming, working

    while working:
        if action == 'FILE':
            file_path = filedialog.askopenfilename(filetypes=[("MP3 files", "*.mp3")])
            if file_path:
                client_socket.send(action.encode('utf-8'))
                handshake = client_socket.recv(MESSAGE_SIZE)
                if handshake.decode('utf-8') == 'OK ':
                    print("Sending file...")
                file_name = file_path.split('/')[-1]
                client_socket.send(file_name.encode('utf-8'))
                handshake = client_socket.recv(MESSAGE_SIZE)
                print(handshake.decode('utf-8'))
                # if handshake.decode('utf-8') == 'OK ':
                send_mp3_file(file_path, client_socket)
                # else:
                #     messagebox.showerror("Error", "Server rejected the file.")
            action = None  # Reset the action after execution

        elif action == 'STREAM':
            if streaming:
                messagebox.showinfo("Info", "Already streaming")
            else:
                streaming = True
            action = None

        elif action == 'STOP':
            if streaming:
                streaming = False
                messagebox.showinfo("Info", "Streaming stopped")
            else:
                messagebox.showinfo("Info", "Not streaming")
            action = None

        elif action == 'QUEUECHANGE':
            handle_queue_change(client_socket)
            action = None

        elif action == "QUEUE":
            messagebox.showinfo("Queue", "\n".join(file_queue))
            action = None

        elif action == 'HELLO':
            client_socket.send(action.encode('utf-8'))
            handshake = client_socket.recv(MESSAGE_SIZE)
            client_socket.send("Hello".encode('utf-8'))
            action = None

        elif action == "END":
            client_socket.send(action.encode('utf-8'))
            working = False
            streaming = False
            client_streaming_socket.close()
            client_queue_socket.close()
            root.after(0,root.destroy)
            break

        else:
            # Wait for next action
            threading.Event().wait(0.5)
def update_queue_display():
    """Update the Listbox with the current queue"""
    queue_listbox.delete(0, tk.END)  # Clear the current listbox items
    for track in file_queue:
        queue_listbox.insert(tk.END, track)  # Insert each track into the listbox
    root.after(1000, update_queue_display)  # Schedule this function to run every second


if __name__ == '__main__':
    try:
        # Setup sockets
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
        handle_thread = threading.Thread(target=handle_action, args=(client_socket, client_streaming_socket, client_queue_socket))
        handle_thread.start()
        
        root = tk.Tk()
        root.title("Music Streaming Client")

        # Action buttons
        tk.Button(root, text="Upload File", command=lambda: set_action('FILE')).pack(pady=5)
        tk.Button(root, text="Start Streaming", command=lambda: set_action('STREAM')).pack(pady=5)
        tk.Button(root, text="Stop Streaming", command=lambda: set_action('STOP')).pack(pady=5)
        tk.Button(root, text="Queue Change", command=lambda: set_action('QUEUECHANGE')).pack(pady=5)
        tk.Button(root, text="View Queue", command=lambda: set_action('QUEUE')).pack(pady=5)
        tk.Button(root, text="Hello", command=lambda: set_action('HELLO')).pack(pady=5)
        tk.Button(root, text="Exit", command=lambda: set_action('END')).pack(pady=10)
        queue_listbox = tk.Listbox(root, height = 10, width = 50)
        queue_listbox.pack(pady=10)

        root.after(1000, update_queue_display)
        root.mainloop()
        

    except Exception as e:
        print(f"Error: {e}")
    finally:
        client_socket.close()
        client_streaming_socket.close()
        client_queue_socket.close()
