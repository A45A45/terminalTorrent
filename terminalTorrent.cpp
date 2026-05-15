#include <iostream>
#include <thread>
#include <chrono>
#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/torrent_status.hpp>

namespace lt = libtorrent;

int main(int argc, char const* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./mini_torrent <magnet_url_or_file>" << std::endl;
        return 1;
    }

    lt::settings_pack pack;
    // Set a user agent so trackers don't block us
    pack.set_str(lt::settings_pack::user_agent, "MiniTorrent/1.0");

    lt::session ses(pack);
    lt::add_torrent_params p;

    // Save the download in the current directory
    p.save_path = ".";

    // Check if input is a magnet link or a file path
    std::string input = argv[1];
    if (input.substr(0, 7) == "magnet:") {
        p = lt::parse_magnet_uri(input);
        p.save_path = ".";
    } else {
        // Basic file loading (simplified)
        p.ti = std::make_shared<lt::torrent_info>(input);
    }

    lt::torrent_handle h = ses.add_torrent(p);

    std::cout << "Starting download... (Press Ctrl+C to stop)\n";

    while (true) {
        lt::torrent_status s = h.status();

        // Print progress to terminal
        std::cout << "\rProgress: " << (s.progress * 100) << "% "
                  << "| Speed: " << (s.download_payload_rate / 1000) << " kB/s "
                  << "| Peers: " << s.num_peers
                  << "   " << std::flush;

        if (s.is_seeding) {
            std::cout << "\nDownload complete. Seeding..." << std::endl;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return 0;
}
