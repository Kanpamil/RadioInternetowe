# RadioInternetowe
#### Projekt na przedmiot Sieci komputerowe 2
#### semestr 5 grupa L5
- Kamil Kaczmarek 155971
- Jakub Butkiewicz 155833

## How to run
1. type make in the terminal to compile project
2. Run server using: ./bin/server.out
3. Run client using: python3 ./scripts/klient.py
   
### Some libraries if code doesnt work
- sudo apt-get install ffmpeg
- sudo apt-get install libmodplug1
- sudo apt-get install libasound2-dev
- sudo apt-get install pkg-config
- sudo apt-get install libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev
- pip install pydub 
- pip install simpleaudio
- pip install tk or sudo apt install python3-tk

### Checking ports
 - sudo netstat -tulnp | grep :110*

#### mutexes to do in server
- skip
- filequeue
- running
- trackmoment

#### mutexes to do in client
- file_queue = []
- streaming = False
- playing = False
- working = True


