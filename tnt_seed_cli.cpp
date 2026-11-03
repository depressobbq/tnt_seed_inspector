#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <algorithm>
#include <map>

struct TNT_RNG {
    uint32_t seed;
    TNT_RNG(uint32_t s) : seed(s) {}
    inline uint32_t step() {
        seed = (seed * 10001u + 100001u) & 0xFFFFFFFFu;
        return seed;
    }
    inline uint32_t rand_range(uint32_t min_v, uint32_t max_v) {
        if (min_v == max_v) return min_v;
        if (max_v < min_v) std::swap(min_v, max_v);
        uint32_t val = step();
        uint32_t range = (max_v - min_v) + 1u;
        return min_v + (val % range);
    }
};

const char PIECES[7] = { 'I', 'J', 'L', 'O', 'S', 'T', 'Z' };

struct RunResult {
    uint32_t seed;
    char hold;
    std::vector<char> bag;
    int max_drought_I;
    int max_drought_O;
};

RunResult gen(uint32_t seed) {
    TNT_RNG rng(seed);
    RunResult res;
    res.seed = seed;
    res.hold = PIECES[rng.rand_range(0, 6)];

    std::vector<int> pool;
    for (int p = 0; p < 7; ++p)
        for (int i = 0; i < 9; ++i)
            pool.push_back(p);

    for (int rem = 63; rem > 0; --rem) {
        uint32_t pick = rng.rand_range(0, rem - 1);
        res.bag.push_back(PIECES[pool[pick]]);
        pool[pick] = pool[rem - 1];
    }

    int last_i = -1, max_i = 0;
    int last_o = -1, max_o = 0;
    for (int i = 0; i < 63; ++i) {
        if (res.bag[i] == 'I') {
            if (last_i != -1) max_i = std::max(max_i, i - last_i - 1);
            else max_i = i;
            last_i = i;
        }
        if (res.bag[i] == 'O') {
            if (last_o != -1) max_o = std::max(max_o, i - last_o - 1);
            else max_o = i;
            last_o = i;
        }
    }
    res.max_drought_I = max_i;
    res.max_drought_O = max_o;
    return res;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "The New Tetris (N64) Seed & Bag CLI\n";
        std::cout << "Usage:\n";
        std::cout << "  " << argv[0] << " frame <frame_number>\n";
        std::cout << "  " << argv[0] << " seed <hex_or_dec_seed>\n";
        std::cout << "  " << argv[0] << " search <hold_char> <next_piece_string> [start_frame] [end_frame]\n";
        std::cout << "Example: " << argv[0] << " search L TOTZO 0 60000\n";
        return 0;
    }

    std::string cmd = argv[1];
    if (cmd == "frame" && argc >= 3) {
        uint32_t f = std::stoul(argv[2]);
        uint32_t s = (f * 781250u) & 0xFFFFFFFFu;
        RunResult r = gen(s);
        std::cout << "Frame: " << f << " | Seed: 0x" << std::hex << std::uppercase << s << std::dec << " (" << s << ")\n";
        std::cout << "Hold: [" << r.hold << "]\n";
        std::cout << "Upcoming Preview (14): ";
        for (int i = 0; i < 14; ++i) std::cout << r.bag[i] << " ";
        std::cout << "\nFull 63-Bag: ";
        for (char c : r.bag) std::cout << c << " ";
        std::cout << "\nMax Droughts: I=" << r.max_drought_I << ", O=" << r.max_drought_O << "\n";
    } else if (cmd == "seed" && argc >= 3) {
        std::string s_str = argv[2];
        uint32_t s = (s_str.find("0x") == 0) ? std::stoul(s_str, nullptr, 16) : std::stoul(s_str);
        RunResult r = gen(s);
        std::cout << "Seed: 0x" << std::hex << std::uppercase << s << std::dec << " (" << s << ")\n";
        std::cout << "Hold: [" << r.hold << "]\n";
        std::cout << "Upcoming Preview (14): ";
        for (int i = 0; i < 14; ++i) std::cout << r.bag[i] << " ";
        std::cout << "\nFull 63-Bag: ";
        for (char c : r.bag) std::cout << c << " ";
        std::cout << "\nMax Droughts: I=" << r.max_drought_I << ", O=" << r.max_drought_O << "\n";
    } else if (cmd == "search" && argc >= 4) {
        char hold = toupper(argv[2][0]);
        std::string next_str = argv[3];
        std::vector<char> next_pieces;
        for (char c : next_str) if (isalpha(c)) next_pieces.push_back(toupper(c));

        uint32_t start_f = (argc >= 5) ? std::stoul(argv[4]) : 0;
        uint32_t end_f = (argc >= 6) ? std::stoul(argv[5]) : 60000;

        std::cout << "Searching frames " << start_f << " to " << end_f << " for Hold=" << hold << " Next=" << next_str << "...\n";
        int matches = 0;
        for (uint32_t f = start_f; f <= end_f; ++f) {
            uint32_t s = (f * 781250u) & 0xFFFFFFFFu;
            RunResult r = gen(s);
            if (hold != '?' && r.hold != hold) continue;
            bool ok = true;
            for (size_t i = 0; i < next_pieces.size(); ++i) {
                if (i >= r.bag.size() || r.bag[i] != next_pieces[i]) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                std::cout << "  -> MATCH: Frame " << f << " | Seed: 0x" << std::hex << std::uppercase << s << std::dec << " | Hold: " << r.hold << " | Next: ";
                for (int i = 0; i < 8 && i < (int)r.bag.size(); ++i) std::cout << r.bag[i] << " ";
                std::cout << "\n";
                matches++;
                if (matches >= 20) {
                    std::cout << "  (Showing first 20 matches...)\n";
                    break;
                }
            }
        }
        if (matches == 0) std::cout << "No matches found.\n";
    }
    return 0;
}
