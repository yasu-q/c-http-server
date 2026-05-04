# http-server
A simple http server implented in C. Created as an educational project. Not meant for actual use. 

> Built for Linux and has only been tested on Ubuntu 24.04.04 on WSL2.

## Overview
http-server allows you to serve basic static files over http. It supports *only* GET requests, and it can serve files from a specified directory. It also has some basic error handling for common HTTP errors. It can serve basic static files (html, css, js, png, jpg, and even mp4 somehow!). 

http-server also supports some basic terminal commands. While the program is running, type `help` for a list of commands

## Building and Running
Requires `gcc`
### 1. Clone the repository
```bash
git clone <repository-url>
```
### 2. Build the project using the provided Makefile
```bash
make bin/http-server
```
### 3. Run the server
```bash
./bin/http-server <directory-to-serve> <port>
```
e.g: serve the included `content/` directory on port 8080:

```bash
./bin/http-server ./content 8080
```

Running with no arguments defaults to port `3490` and the `./content` directory.

Then open a browser and navigate to `http://localhost:<port>/`

## Usage
Once the server is running, you can start accepting connections. That's all. You can also play around with the commands to see information regarding the server and connections.

| Command       | Description                              |
|---------------|------------------------------------------|
| `help`        | List all available commands              |
| `server`      | Show server info (IP, port, backlog)     |
| `connections` | Show current active connections          |
| `uptime`      | Show how long the server has been running|
| `notifs`      | Toggle connection/request notifications  |
| `quit`        | Shut down the server                     |

## Project Structure

```bash
├── Makefile
├── README.md
├── bin                     # compiled binaries. generated
├── include
│   ├── arraylist.h         # array list definitions
│   ├── http-server.h       # server types
│   ├── http-util.h         # HTTP types
│   └── sock-util.h         # socket utilities
├── src
│   ├── http-server.c       # primary server logic, connection handling
│   └── util
│       ├── arraylist.c     # array list used throughout the project
│       ├── http-util.c     # HTTP parsing and response 
│       └── sock-util.c     # handle socket operations
```