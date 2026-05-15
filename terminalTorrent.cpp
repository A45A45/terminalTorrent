#include <iostream>
#include <thread>
#include <vector>
#include <filesystem>
#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/torrent_info.hpp>
#include <ncurses.h>

namespace lt = libtorrent;
namespace fs = std::filesystem;

struct AppState {
    lt::session ses;
    std::vector<lt::torrent_handle> handles;
    int selected_idx = 0;
    bool running = true;
    std::string watch_dir;
};

void sync_from_disk(AppState& state) {
    for (auto const& entry : fs::directory_iterator(state.watch_dir)) {
        if (entry.path().extension() == ".torrent") {
            try {
                lt::add_torrent_params p;
                p.ti = std::make_shared<lt::torrent_info>(entry.path().string());
                p.save_path = state.watch_dir;

                // Don't auto-start: add it in a paused state
                p.flags |= lt::torrent_flags::paused;

                bool exists = false;
                for(auto& h : state.handles) {
                    if(h.status().info_hashes == p.ti->info_hashes()) exists = true;
                }

                if(!exists) state.handles.push_back(state.ses.add_torrent(p));
            } catch (...) {}
        }
    }
}

void run_tui(AppState& state) {
    initscr();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    wtimeout(stdscr, 500);

    while (state.running) {
        sync_from_disk(state);
        clear();

        mvprintw(0, 1, "LOCAL WATCHER: %s", state.watch_dir.c_str());
        mvprintw(1, 1, "s: START/PAUSE | d: REMOVE | q: QUIT");
        mvprintw(2, 0, "--------------------------------------------------------------------------");

        for (int i = 0; i < (int)state.handles.size(); ++i) {
            auto s = state.handles[i].status();
            if (i == state.selected_idx) attron(A_REVERSE);

            const char* status_str = (s.flags & lt::torrent_flags::paused) ? "PAUSED " : "ACTIVE ";
            mvprintw(4 + i, 1, "%-30s | %s | %5.1f%%",
                     s.name.substr(0, 30).c_str(), status_str, s.progress * 100);

            if (i == state.selected_idx) attroff(A_REVERSE);
        }

        int ch = getch();
        switch (ch) {
            case KEY_UP: if (state.selected_idx > 0) state.selected_idx--; break;
            case KEY_DOWN: if (state.selected_idx < (int)state.handles.size() - 1) state.selected_idx++; break;
            case 's': // Toggle Start/Pause
                if (!state.handles.empty()) {
                    auto& h = state.handles[state.selected_idx];
                    if (h.status().flags & lt::torrent_flags::paused) h.resume();
                    else h.pause();
                }
                break;
            case 'd': // Remove from list
                if (!state.handles.empty()) {
                    state.ses.remove_torrent(state.handles[state.selected_idx]);
                    state.handles.erase(state.handles.begin() + state.selected_idx);
                }
                break;
            case 'q': state.running = false; break;
        }
        refresh();
    }
    endwin();
}

int main() {
    const char* home = std::getenv("HOME");
    std::string d_path = (home) ? std::string(home) + "/Downloads" : "./";

    AppState state;
    state.watch_dir = d_path;

    run_tui(state);
    return 0;
}
