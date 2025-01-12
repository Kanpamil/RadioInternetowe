# RadioInternetowe
Projekt na przedmot Sieci komputerowe 2


to do
operations on queue - swap and skip
after skip send info about streaming through queuesocket
interface client

## How to run
1. type make in the terminal to compile project
2. Run server using: ./bin/server.out
3. Run client using: python3 ./scripts/klient.py
   
### Some libraries if code doesnt work
- sudo apt-get install ffmpeg
- sudo apt install libtag1-dev
- sudo apt-get install libmodplug1
- pip install pydub 
- sudo apt-get install libasound2-dev
- pip install simpleaudio

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


