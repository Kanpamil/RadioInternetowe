import socket
import pygame
import io

SERVER_IP = "127.0.0.1"
SERVER_PORT = 8080
BUFFER_SIZE = 4096

def stream_audio():
    pygame.mixer.init()
    audio_data = io.BytesIO()

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((SERVER_IP, SERVER_PORT))
            print("Połączono z serwerem.")

            while True:
                # Odbierz nagłówek
                header = sock.recv(8).decode()
                if not header:
                    break

                message_type = header[:4]  # Typ wiadomości (META, DATA)
                message_length = int(header[4:])  # Długość danych

                # Odbierz dane na podstawie długości podanej w nagłówku
                body = sock.recv(message_length)

                if message_type == "META":
                    print(f"Otrzymano metadane: {body.decode()}")
                elif message_type == "DATA":
                    audio_data.write(body)

                    # Jeśli audio nie gra, rozpocznij odtwarzanie
                    if not pygame.mixer.music.get_busy():
                        audio_data.seek(0)
                        pygame.mixer.music.load(audio_data)
                        pygame.mixer.music.play()
                        audio_data.seek(0, io.SEEK_END)  # Przygotuj bufor na nowe dane
                else:
                    print("Nieznany typ wiadomości.")
                    break

            while pygame.mixer.music.get_busy():
                pass  # Oczekuj na zakończenie odtwarzania

    except Exception as e:
        print(f"Błąd: {e}")
    finally:
        pygame.mixer.quit()

if __name__ == "__main__":
    stream_audio()
