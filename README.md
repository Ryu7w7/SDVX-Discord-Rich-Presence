# SDVX Rich Presence

This DLL monitors the game's state and updates your Discord status in real-time to show what song you are playing, the current game mode, and menu navigation.

## Features

- **Real-Time Status**: Displays whether you are in the Menu, Selecting a song, Playing, or viewing Results.
- **Detailed Game Modes**: Shows specific modes like Premium Time, Blaster Start, Megamix Battle, Arena Battle, and Hexa Diver.

## Requisites

To compile the source code, you will need a C++ compiler for Windows:
- **MinGW-w64** (specifically `g++`) 
- `make` (optional, but a Makefile is provided for convenience)

## Setup & Compilation

Before compiling, you **must** configure your Discord Application ID.

1. Create a new Application in the [Discord Developer Portal](https://discord.com/developers/applications).
2. Open `main.cpp` and replace the placeholder with your Application ID:
   ```cpp
   CLIENT_ID = "YOUR_CLIENT_ID_HERE";
   ```
3. *(Optional)* If you host your jackets and `music_db.xml` on a web server, you can uncomment and configure the URLs under the `REMOTE SERVER CONFIGURATION` section in `main.cpp`. If left empty, the plugin will safely default to reading local files and use a generic *sdvx_nabla* image.
4. Open a terminal in this folder and compile the project by running:
   ```bash
   make
   ```
   *(If you don't have make, you can run the g++ command found inside the Makefile).*

5. The compilation will generate an `sdvxrpc.dll` file.

## Usage

Once compiled, the `sdvxrpc.dll` needs to be loaded by the game. Put sdvxrpc.dll in ur root sdvx folder and in spicecfg write sdvxrpc.dll in Inject DLL Hooks

*Make sure your Discord Desktop app is open and running before starting the game for the Rich Presence to connect properly.*
