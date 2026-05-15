# terminalTorrent
Minimal terminal based torrent client written in C++
Requires libtorrent development library

##Installation 

git clone https://github.com/A45A45/terminalTorrent
cd mini-torrent

g++ -std=c++17 terminalTorrent.cpp -o terminalTorrent -ltorrent-rasterbar -lpthread

##Usage
./terminalTorrent "magnetlink"

./terminalTorrent file.torrent
