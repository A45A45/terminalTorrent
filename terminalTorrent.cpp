#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/torrent_info.hpp>

#include <ncurses.h>

#include <filesystem>
#include <unordered_map>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;
namespace lt = libtorrent;

struct TorrentEntry {
    fs::path torrentPath;
    lt::torrent_handle handle;
    bool loaded = false;
};

std::string humanBytes(float bytes) {
    const char* suffixes[] = {"B/s", "KB/s", "MB/s", "GB/s"};

    int suffix = 0;

    while (bytes >= 1024.0f && suffix < 3) {
        bytes /= 1024.0f;
        suffix++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1)
        << bytes << " " << suffixes[suffix];

    return oss.str();
}

std::string stateToString(lt::torrent_status::state_t s) {
    switch (s) {
        case lt::torrent_status::checking_files: return "checking";
        case lt::torrent_status::downloading_metadata: return "metadata";
        case lt::torrent_status::downloading: return "downloading";
        case lt::torrent_status::finished: return "finished";
        case lt::torrent_status::seeding: return "seeding";
        case lt::torrent_status::checking_resume_data: return "resume";
        default: return "unknown";
    }
}

class TorrentApp {
private:
    lt::session session;
    std::vector<TorrentEntry> torrents;

    int selected = 0;

    std::chrono::steady_clock::time_point lastScan;

public:
    TorrentApp() {
        lt::settings_pack pack;

        pack.set_int(lt::settings_pack::alert_mask,
                     lt::alert_category::status |
                     lt::alert_category::error |
                     lt::alert_category::storage);

        session.apply_settings(pack);

        lastScan = std::chrono::steady_clock::now();
    }

    fs::path downloadsDir() {
        const char* home = std::getenv("HOME");
        return fs::path(home) / "Downloads";
    }

    bool existsAlready(const fs::path& p) {
        for (const auto& t : torrents) {
            if (t.torrentPath == p)
                return true;
        }
        return false;
    }

    void scanDownloads() {
        auto now = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - lastScan
        ).count();

        if (elapsed < 5)
            return;

        lastScan = now;

        fs::path dir = downloadsDir();

        if (!fs::exists(dir))
            return;

        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file())
                continue;

            if (entry.path().extension() != ".torrent")
                continue;

            if (existsAlready(entry.path()))
                continue;

            try {
                lt::error_code ec;

                auto info = std::make_shared<lt::torrent_info>(
                    entry.path().string(),
                    ec
                );

                if (ec)
                    continue;

                lt::add_torrent_params params;
                params.ti = info;
                params.save_path = downloadsDir().string();

                auto handle = session.add_torrent(params, ec);

                if (ec)
                    continue;

                TorrentEntry t;
                t.torrentPath = entry.path();
                t.handle = handle;
                t.loaded = true;

                torrents.push_back(t);
            }
            catch (...) {
            }
        }
    }

    void togglePause(int idx) {
        if (idx < 0 || idx >= (int)torrents.size())
            return;

        auto& t = torrents[idx];

        if (!t.loaded)
            return;

        auto status = t.handle.status();

        if (status.flags & lt::torrent_flags::paused)
            t.handle.resume();
        else
            t.handle.pause();
    }

    void deleteTorrent(int idx) {
        if (idx < 0 || idx >= (int)torrents.size())
            return;

        auto t = torrents[idx];

        if (t.loaded)
            session.remove_torrent(t.handle);

        try {
            fs::remove(t.torrentPath);
        }
        catch (...) {
        }

        torrents.erase(torrents.begin() + idx);

        if (selected >= (int)torrents.size())
            selected = torrents.size() - 1;

        if (selected < 0)
            selected = 0;
    }

    void drawHeader() {
        mvprintw(0, 0,
            "Torrent TUI  |  S Start/Pause  D Delete  Q Quit");

        mvprintw(1, 0,
            "--------------------------------------------------------------------------------");

        mvprintw(2, 0,
            "%-3s %-28s %-10s %-12s %-12s %-10s %-8s",
            "#",
            "Name",
            "Progress",
            "Down",
            "Up",
            "State",
            "Peers"
        );
    }

    void drawTorrents() {
        int row = 4;

        for (int i = 0; i < (int)torrents.size(); ++i) {
            auto& t = torrents[i];

            if (!t.loaded)
                continue;

            auto st = t.handle.status();

            std::string name = st.name;

            if (name.size() > 28)
                name = name.substr(0, 25) + "...";

            float progress = st.progress_ppm / 10000.0f;

            if (i == selected)
                attron(A_REVERSE);

            mvprintw(
                row,
                0,
                "%-3d %-28s %6.1f%%   %-12s %-12s %-10s %-8d",
                i + 1,
                name.c_str(),
                progress,
                humanBytes(st.download_rate).c_str(),
                humanBytes(st.upload_rate).c_str(),
                stateToString(st.state).c_str(),
                st.num_peers
            );

            if (i == selected)
                attroff(A_REVERSE);

            row++;
        }
    }

    void drawFooter() {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);

        mvprintw(rows - 2, 0,
            "Watching: %s",
            downloadsDir().c_str()
        );

        mvprintw(rows - 1, 0,
            "Loaded torrents: %zu",
            torrents.size()
        );
    }

    void render() {
        clear();

        drawHeader();
        drawTorrents();
        drawFooter();

        refresh();
    }

    void handleInput(int ch) {
        switch (ch) {
            case KEY_UP:
                if (selected > 0)
                    selected--;
                break;

            case KEY_DOWN:
                if (selected < (int)torrents.size() - 1)
                    selected++;
                break;

            case 's':
            case 'S':
                togglePause(selected);
                break;

            case 'd':
            case 'D':
                deleteTorrent(selected);
                break;
        }
    }

    void run() {
        initscr();
        noecho();
        cbreak();
        keypad(stdscr, TRUE);
        curs_set(0);
        timeout(200);

        bool running = true;

        while (running) {
            scanDownloads();
            render();

            int ch = getch();

            switch (ch) {
                case 'q':
                case 'Q':
                    running = false;
                    break;

                default:
                    handleInput(ch);
                    break;
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(100)
            );
        }

        endwin();
    }
};

int main() {
    TorrentApp app;
    app.run();
    return 0;
}
