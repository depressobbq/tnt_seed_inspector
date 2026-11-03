#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <algorithm>
#include <map>

// -------------------------------------------------------------
// Core TNT PRNG and Game Logic
// -------------------------------------------------------------

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

const char PIECE_CHARS[7] = { 'I', 'J', 'L', 'O', 'S', 'T', 'Z' };

struct BagResult {
    uint32_t seed;
    uint32_t frame;
    char hold_piece;
    std::vector<char> bag; // 63 pieces
    int max_drought_I;
    int max_drought_O;
    int max_drought_any;
    std::string drought_summary;
    std::string distribution_summary;
};

BagResult generate_bag(uint32_t seed, uint32_t frame = 0) {
    BagResult res;
    res.seed = seed;
    res.frame = frame;

    TNT_RNG rng(seed);

    // Initial Hold piece (outside bag)
    uint32_t hold_idx = rng.rand_range(0, 6);
    res.hold_piece = PIECE_CHARS[hold_idx];

    // 63-piece bag: 9 of each piece
    std::vector<int> pool;
    pool.reserve(63);
    for (int p = 0; p < 7; ++p) {
        for (int i = 0; i < 9; ++i) {
            pool.push_back(p);
        }
    }

    // Fisher-Yates deal
    res.bag.reserve(63);
    for (int remaining = 63; remaining > 0; --remaining) {
        uint32_t pick = rng.rand_range(0, remaining - 1);
        res.bag.push_back(PIECE_CHARS[pool[pick]]);
        pool[pick] = pool[remaining - 1];
    }

    // Drought calculations
    std::map<char, int> last_seen;
    std::map<char, int> max_drought;
    for (char c : PIECE_CHARS) {
        last_seen[c] = -1;
        max_drought[c] = 0;
    }

    for (int i = 0; i < (int)res.bag.size(); ++i) {
        char c = res.bag[i];
        if (last_seen[c] != -1) {
            int d = i - last_seen[c] - 1;
            if (d > max_drought[c]) max_drought[c] = d;
        } else {
            max_drought[c] = i; // drought from start
        }
        last_seen[c] = i;
    }
    for (char c : PIECE_CHARS) {
        int end_d = (int)res.bag.size() - 1 - last_seen[c];
        if (end_d > max_drought[c]) max_drought[c] = end_d;
    }

    res.max_drought_I = max_drought['I'];
    res.max_drought_O = max_drought['O'];
    res.max_drought_any = 0;
    for (auto const& kv : max_drought) {
        if (kv.second > res.max_drought_any) res.max_drought_any = kv.second;
    }

    std::ostringstream ds;
    ds << "Max Droughts -> I: " << res.max_drought_I
       << " | O: " << res.max_drought_O
       << " | J: " << max_drought['J']
       << " | L: " << max_drought['L']
       << " | S: " << max_drought['S']
       << " | T: " << max_drought['T']
       << " | Z: " << max_drought['Z'];
    res.drought_summary = ds.str();

    std::ostringstream dist;
    dist << "Bag Composition: 9 of each tetromino (63 total). Initial Hold: [" 
         << res.hold_piece << "]";
    res.distribution_summary = dist.str();

    return res;
}

// Timing models
const uint32_t N64_CYCLES_PER_FRAME = 781250u; // 46.875 MHz / 60 FPS

uint32_t frame_to_seed(uint32_t frame, int mode) {
    if (mode == 0) {
        // N64 Hardware / Cycle accurate
        return (frame * N64_CYCLES_PER_FRAME) & 0xFFFFFFFFu;
    } else {
        // Static Recomp / PC Mode (Frame incrementing seed)
        return (frame * 10001u + 0x12345u) & 0xFFFFFFFFu;
    }
}

// -------------------------------------------------------------
// Win32 GUI Definitions
// -------------------------------------------------------------

#define IDC_TAB_MODE         101
#define IDC_ED_FRAME         102
#define IDC_ED_SEED_HEX      103
#define IDC_ED_SEED_DEC      104
#define IDC_BTN_GENERATE     105
#define IDC_COMBO_TIMING     106

#define IDC_ED_SEARCH_HOLD   201
#define IDC_ED_SEARCH_NEXT   202
#define IDC_ED_SEARCH_START  203
#define IDC_ED_SEARCH_END    204
#define IDC_BTN_SEARCH       205
#define IDC_LIST_RESULTS     206

#define IDC_ED_BAG_OUTPUT    301
#define IDC_BTN_COPY_SEED    302

HWND g_hMainWnd = NULL;
HWND g_hTimingCombo = NULL;
HWND g_hFrameInput = NULL;
HWND g_hSeedHex = NULL;
HWND g_hSeedDec = NULL;
HWND g_hBagOutput = NULL;

HWND g_hSearchHold = NULL;
HWND g_hSearchNext = NULL;
HWND g_hSearchStart = NULL;
HWND g_hSearchEnd = NULL;
HWND g_hListResults = NULL;

std::vector<BagResult> g_searchResults;

void UpdateBagDisplay(const BagResult& res) {
    std::ostringstream oss;
    oss << "=== THE NEW TETRIS (N64) RUN INSPECTION ===\r\n";
    oss << "Seed (Hex):       0x" << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << res.seed << "\r\n";
    oss << "Seed (Decimal):   " << std::dec << res.seed << "\r\n";
    if (res.frame > 0) {
        oss << "Starting Frame:   " << res.frame << " (~" << (res.frame / 60) << "s into boot)\r\n";
    }
    oss << "Initial Hold:     [" << res.hold_piece << "]\r\n\r\n";

    oss << "Upcoming Next Queue (First 14 pieces):\r\n";
    for (int i = 0; i < 14 && i < (int)res.bag.size(); ++i) {
        oss << " " << (i + 1) << ":" << res.bag[i];
    }
    oss << "\r\n\r\n";

    oss << "Full 63-Piece Bag Sequence:\r\n";
    for (int i = 0; i < (int)res.bag.size(); ++i) {
        oss << res.bag[i] << " ";
        if ((i + 1) % 21 == 0) oss << "\r\n";
    }
    oss << "\r\n\r\n";

    oss << "Drought Analytics:\r\n";
    oss << res.drought_summary << "\r\n\r\n";

    oss << "Square Potential Assessment:\r\n";
    int gold_sq_pairs = 0;
    for (int i = 0; i < 20; ++i) {
        if (res.bag[i] == 'O' || res.bag[i] == 'I') gold_sq_pairs++;
    }
    oss << "Early Golden/Silver candidate density (first 20 pieces): " << gold_sq_pairs << "/20\r\n";
    if (res.max_drought_I <= 7) {
        oss << ">> EXCELLENT RUN SEED: Minimal I-piece drought (max " << res.max_drought_I << " pieces).\r\n";
    } else if (res.max_drought_I >= 15) {
        oss << ">> WARNING: Heavy I-piece drought (up to " << res.max_drought_I << " pieces gap).\r\n";
    }

    SetWindowTextA(g_hBagOutput, oss.str().c_str());

    std::ostringstream sh, sd;
    sh << "0x" << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << res.seed;
    sd << std::dec << res.seed;
    SetWindowTextA(g_hSeedHex, sh.str().c_str());
    SetWindowTextA(g_hSeedDec, sd.str().c_str());
}

void DoGenerate() {
    char buf[128];
    GetWindowTextA(g_hFrameInput, buf, sizeof(buf));
    uint32_t frame = 0;
    try {
        if (strlen(buf) > 0) frame = std::stoul(buf);
    } catch (...) {}

    int timing_mode = SendMessage(g_hTimingCombo, CB_GETCURSEL, 0, 0);
    uint32_t seed = 0;

    // Check if custom hex seed was entered directly
    char hex_buf[64];
    GetWindowTextA(g_hSeedHex, hex_buf, sizeof(hex_buf));
    std::string s_hex = hex_buf;
    if (s_hex.length() > 2 && s_hex.find("0x") == 0) {
        try {
            seed = std::stoul(s_hex, nullptr, 16);
        } catch (...) {
            seed = frame_to_seed(frame, timing_mode);
        }
    } else {
        seed = frame_to_seed(frame, timing_mode);
    }

    BagResult res = generate_bag(seed, frame);
    UpdateBagDisplay(res);
}

void DoSearch() {
    char hold_buf[16];
    char next_buf[64];
    char start_buf[32];
    char end_buf[32];

    GetWindowTextA(g_hSearchHold, hold_buf, sizeof(hold_buf));
    GetWindowTextA(g_hSearchNext, next_buf, sizeof(next_buf));
    GetWindowTextA(g_hSearchStart, start_buf, sizeof(start_buf));
    GetWindowTextA(g_hSearchEnd, end_buf, sizeof(end_buf));

    char target_hold = ' ';
    if (strlen(hold_buf) > 0) {
        target_hold = toupper(hold_buf[0]);
    }

    std::vector<char> target_next;
    for (int i = 0; next_buf[i]; ++i) {
        char c = toupper(next_buf[i]);
        if (c >= 'A' && c <= 'Z') target_next.push_back(c);
    }

    uint32_t start_frame = 0;
    uint32_t end_frame = 60000;
    try {
        if (strlen(start_buf) > 0) start_frame = std::stoul(start_buf);
        if (strlen(end_buf) > 0) end_frame = std::stoul(end_buf);
    } catch (...) {}

    int timing_mode = SendMessage(g_hTimingCombo, CB_GETCURSEL, 0, 0);

    SendMessage(g_hListResults, LB_RESETCONTENT, 0, 0);
    g_searchResults.clear();

    int matches = 0;
    for (uint32_t f = start_frame; f <= end_frame; ++f) {
        uint32_t s = frame_to_seed(f, timing_mode);
        BagResult r = generate_bag(s, f);

        bool match = true;
        if (target_hold != ' ' && r.hold_piece != target_hold) {
            match = false;
        }
        if (match) {
            for (size_t i = 0; i < target_next.size(); ++i) {
                if (i >= r.bag.size() || r.bag[i] != target_next[i]) {
                    match = false;
                    break;
                }
            }
        }

        if (match) {
            g_searchResults.push_back(r);
            std::ostringstream item;
            item << "Frame " << f << " | Seed: 0x" 
                 << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << s 
                 << " | Hold: " << r.hold_piece << " | Next: ";
            for (int k = 0; k < 6 && k < (int)r.bag.size(); ++k) item << r.bag[k] << " ";
            SendMessageA(g_hListResults, LB_ADDSTRING, 0, (LPARAM)item.str().c_str());
            matches++;
            if (matches >= 200) break; // limit results list
        }
    }

    if (matches == 0) {
        SendMessageA(g_hListResults, LB_ADDSTRING, 0, (LPARAM)"No matching seed found in this frame window. Try adjusting pieces or expanding frame range.");
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        // Group 1: Frame / Seed Controls
        HWND grp1 = CreateWindowA("BUTTON", "1. Seed & Starting Frame Generator", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            10, 10, 480, 150, hWnd, NULL, NULL, NULL);
        SendMessage(grp1, WM_SETFONT, (WPARAM)hFont, TRUE);

        CreateWindowA("STATIC", "Hardware Timing:", WS_CHILD | WS_VISIBLE, 25, 35, 110, 20, hWnd, NULL, NULL, NULL);
        g_hTimingCombo = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            140, 32, 220, 120, hWnd, (HMENU)IDC_COMBO_TIMING, NULL, NULL);
        SendMessageA(g_hTimingCombo, CB_ADDSTRING, 0, (LPARAM)"N64 Hardware (46.875 MHz)");
        SendMessageA(g_hTimingCombo, CB_ADDSTRING, 0, (LPARAM)"Static Recomp / PC Port");
        SendMessage(g_hTimingCombo, CB_SETCURSEL, 0, 0);

        CreateWindowA("STATIC", "Start Frame #:", WS_CHILD | WS_VISIBLE, 25, 65, 110, 20, hWnd, NULL, NULL, NULL);
        g_hFrameInput = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "1200", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            140, 62, 100, 22, hWnd, (HMENU)IDC_ED_FRAME, NULL, NULL);

        CreateWindowA("BUTTON", "Generate Bag", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            260, 62, 100, 24, hWnd, (HMENU)IDC_BTN_GENERATE, NULL, NULL);

        CreateWindowA("STATIC", "Seed (Hex):", WS_CHILD | WS_VISIBLE, 25, 95, 100, 20, hWnd, NULL, NULL, NULL);
        g_hSeedHex = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "0x00000000", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            140, 92, 120, 22, hWnd, (HMENU)IDC_ED_SEED_HEX, NULL, NULL);

        CreateWindowA("STATIC", "Seed (Dec):", WS_CHILD | WS_VISIBLE, 25, 125, 100, 20, hWnd, NULL, NULL, NULL);
        g_hSeedDec = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "0", WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL,
            140, 122, 120, 22, hWnd, (HMENU)IDC_ED_SEED_DEC, NULL, NULL);

        CreateWindowA("BUTTON", "Copy Hex Seed", WS_CHILD | WS_VISIBLE,
            280, 92, 120, 24, hWnd, (HMENU)IDC_BTN_COPY_SEED, NULL, NULL);

        // Group 2: Reverse Seed Search (Anti-Cheat / Optimization)
        HWND grp2 = CreateWindowA("BUTTON", "2. Reverse Seed Search (Find Seed from In-Game Pieces)", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            10, 170, 480, 190, hWnd, NULL, NULL, NULL);
        SendMessage(grp2, WM_SETFONT, (WPARAM)hFont, TRUE);

        CreateWindowA("STATIC", "Hold Piece:", WS_CHILD | WS_VISIBLE, 25, 195, 80, 20, hWnd, NULL, NULL, NULL);
        g_hSearchHold = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "L", WS_CHILD | WS_VISIBLE | ES_UPPERCASE,
            110, 192, 40, 22, hWnd, (HMENU)IDC_ED_SEARCH_HOLD, NULL, NULL);

        CreateWindowA("STATIC", "Next Pieces:", WS_CHILD | WS_VISIBLE, 170, 195, 80, 20, hWnd, NULL, NULL, NULL);
        g_hSearchNext = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "T O T Z O", WS_CHILD | WS_VISIBLE | ES_UPPERCASE,
            260, 192, 130, 22, hWnd, (HMENU)IDC_ED_SEARCH_NEXT, NULL, NULL);

        CreateWindowA("STATIC", "Frame Window:", WS_CHILD | WS_VISIBLE, 25, 225, 90, 20, hWnd, NULL, NULL, NULL);
        g_hSearchStart = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "0", WS_CHILD | WS_VISIBLE,
            120, 222, 60, 22, hWnd, (HMENU)IDC_ED_SEARCH_START, NULL, NULL);
        CreateWindowA("STATIC", "to", WS_CHILD | WS_VISIBLE, 190, 225, 20, 20, hWnd, NULL, NULL, NULL);
        g_hSearchEnd = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "60000", WS_CHILD | WS_VISIBLE,
            215, 222, 70, 22, hWnd, (HMENU)IDC_ED_SEARCH_END, NULL, NULL);

        CreateWindowA("BUTTON", "Search Frame/Seed", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            300, 221, 140, 24, hWnd, (HMENU)IDC_BTN_SEARCH, NULL, NULL);

        g_hListResults = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
            25, 255, 450, 95, hWnd, (HMENU)IDC_LIST_RESULTS, NULL, NULL);

        // Group 3: Bag Output & Drought Analytics
        HWND grp3 = CreateWindowA("BUTTON", "3. 63-Piece Bag Inspector & Drought Analytics", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            500, 10, 480, 470, hWnd, NULL, NULL, NULL);
        SendMessage(grp3, WM_SETFONT, (WPARAM)hFont, TRUE);

        g_hBagOutput = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            515, 30, 450, 435, hWnd, (HMENU)IDC_ED_BAG_OUTPUT, NULL, NULL);

        // Apply GUI font to all children
        EnumChildWindows(hWnd, [](HWND hChild, LPARAM lp) -> BOOL {
            SendMessage(hChild, WM_SETFONT, (WPARAM)lp, TRUE);
            return TRUE;
        }, (LPARAM)hFont);

        // Run initial default generation
        DoGenerate();
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDC_BTN_GENERATE) {
            DoGenerate();
        } else if (id == IDC_BTN_SEARCH) {
            DoSearch();
        } else if (id == IDC_BTN_COPY_SEED) {
            char buf[64];
            GetWindowTextA(g_hSeedHex, buf, sizeof(buf));
            if (OpenClipboard(hWnd)) {
                EmptyClipboard();
                HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, strlen(buf) + 1);
                if (hGlob) {
                    memcpy(GlobalLock(hGlob), buf, strlen(buf) + 1);
                    GlobalUnlock(hGlob);
                    SetClipboardData(CF_TEXT, hGlob);
                }
                CloseClipboard();
                MessageBoxA(hWnd, "Seed copied to clipboard! You can paste this directly into your game seed mod.", "Copied", MB_OK | MB_ICONINFORMATION);
            }
        } else if (id == IDC_LIST_RESULTS && HIWORD(wParam) == LBN_SELCHANGE) {
            int sel = SendMessage(g_hListResults, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)g_searchResults.size()) {
                UpdateBagDisplay(g_searchResults[sel]);
            }
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "TNT_Seed_Tool_Class";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassA(&wc);

    HWND hWnd = CreateWindowExA(
        0,
        "TNT_Seed_Tool_Class",
        "The New Tetris (N64) - Seed Generator & Bag Inspector",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 1005, 525,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) return 0;
    g_hMainWnd = hWnd;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
