# terminalTorrent
Minimal terminal based torrent client written in C++

Requires libtorrent-rasterbar, ncurses, C++17

## Installation 

git clone https://github.com/A45A45/terminalTorrent


g++ -std=c++17 terminalTorrent.cpp -o terminalTorrent -ltorrent-rasterbar -lncurses -lpthread

## Usage

Run ./terminalTorrent

Scans the ~/Downloads directory and shows up in the TUI from which the user can manage their .torrent files
