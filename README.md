# The New Tetris (N64) Seed Inspector & Bag Predictor

A standalone seed generator, reverse seed search engine, and 63-piece bag inspector for ***The New Tetris*** (Nintendo 64 / PC Recompilation).

This tool reverses and models the internal 32-bit PRNG and piece bag dealing algorithms from the original N64 game code (`tnt64-recomp`), allowing runners and researchers to analyze starting frames, verify run authenticity (anti-cheat), and calculate piece distributions.

---

## Features

* **Starting Frame $\rightarrow$ Seed Mapping:**
  * **N64 Hardware Timing:** Simulates the $46.875\text{ MHz}$ CPU cycle counter ($\approx 781,250\text{ cycles/frame}$ at 60 FPS) to map boot frames directly to initial seeds.
  * **Static Recomp / PC Timing:** Mode for native PC ports.
  * **Direct Seed Input:** Enter any 32-bit hex (e.g., `0x37E11D60`) or decimal seed.
  * **One-Click Copy:** Easily copy seeds to clipboard for practice and seeded run mods.

* **Reverse Seed Search (Anti-Cheat & Verification):**
  * Given just the **initial Hold piece** and the first **4–6 visible Next pieces** (e.g. Hold: `S`, Next: `Z T J S T`), searches up to hundreds of thousands of frames per second to pinpoint the exact starting frame and seed.
  * Useful for verifying whether a speedrun or high-score submission started on an authentic, achievable frame.

* **63-Piece Bag Inspector & Drought Analytics:**
  * Shows the **Initial Hold Piece** (generated independently outside the bag).
  * Previews the upcoming piece queue (first 14 pieces).
  * Displays the entire **63-piece bag sequence** ($9 \times 7$ pieces).
  * Computes **piece drought analytics** (tracking maximum gaps for `I`, `O`, `J`, `L`, `S`, `T`, `Z`).
  * Calculates early square-building candidate density (crucial for Golden Square / Silver Square optimization).

---

## Technical Background (How The New Tetris RNG Works)

Reverse-engineered from the decompiled N64 game logic:

1. **Seed Initialization (`func_800A3550`):**
   * The game calls N64 OS routine `osGetTime()`, which reads the hardware cycle counter (`Count` register running at $46.875\text{ MHz}$).
   * The lower 32 bits of this timestamp are stored as the initial PRNG seed at RAM address `0x80129340`.

2. **Core PRNG (`func_800A35AC`):**
   * The game uses a 32-bit Linear Congruential Generator (LCG):
     $$\text{seed}_{n+1} = (\text{seed}_n \times 10001 + 100001) \pmod{2^{32}}$$

3. **Range Selection (`func_800A35EC`):**
   * Computes bounded random integers:
     $$\text{rand\_range}(\min, \max) = \min + (\text{PRNG\_step}() \pmod{(\max - \min + 1)})$$

4. **Piece Generation:**
   * **Hold Piece:** Selected at game start via `rand_range(0, 6)`.
   * **63-Piece Bag:** Contains exactly 9 copies of each of the 7 tetrominoes ($9 \times 7 = 63$). The pieces are shuffled and dealt from this pool before a new bag is generated.

---

## Binaries Included

* `tnt_seed_inspector.exe`: Standalone Windows GUI executable (zero external DLL dependencies, runs natively on Windows and under Wine on Linux).
* `tnt_seed_tool`: Fast native Linux CLI executable for terminal usage.

---

## Usage

### 1. Windows / Wine GUI (`tnt_seed_inspector.exe`)

* **On Windows:** Double-click `tnt_seed_inspector.exe`.
* **On Linux (via Wine):**
  ```bash
  wine ./tnt_seed_inspector.exe
  ```

#### In the GUI:
1. **To inspect a starting frame:** Enter the frame number (e.g. `1200`) and click **Generate Bag**.
2. **To inspect a specific seed:** Type or paste a hex seed (e.g. `0x37E11D60`) into the **Seed (Hex)** field and click **Generate Bag**.
3. **To reverse-search a seed from gameplay:**
   * Enter the **Hold Piece** letter (e.g. `S`).
   * Enter the **Next Pieces** seen in the preview (e.g. `Z T J S T`).
   * Set the frame search window (default: `0` to `60000`).
   * Click **Search Frame/Seed**.
   * Click any result in the list to inspect its full bag and drought metrics.

---

### 2. Linux CLI Tool (`tnt_seed_tool`)

```bash
# Inspect a specific starting frame
./tnt_seed_tool frame 1200

# Inspect a specific 32-bit seed (Hex or Dec)
./tnt_seed_tool seed 0x37E11D60

# Reverse search: Hold piece 'S', Next pieces 'ZTJST' between frames 1000 and 1500
./tnt_seed_tool search S ZTJST 1000 1500
```

---

## Compiling from Source

### Requirements
* **For Linux CLI:** `g++` (GCC) with C++11 or newer.
* **For Windows GUI (Cross-compiling on Linux):** `mingw-w64` (`x86_64-w64-mingw32-g++`).

### Build Everything:
```bash
./build.sh
```

### Manual Build Commands:
* **Linux CLI:**
  ```bash
  g++ -O3 tnt_seed_cli.cpp -o tnt_seed_tool && strip tnt_seed_tool
  ```
* **Windows Standalone GUI (`.exe`):**
  ```bash
  x86_64-w64-mingw32-g++ -O3 -mwindows -static tnt_seed_inspector_gui.cpp -o tnt_seed_inspector.exe -lcomctl32
  x86_64-w64-mingw32-strip tnt_seed_inspector.exe
  ```

---

## Integrating Seeded Runs into `tnt64-recomp`

To force a specific seed in the `tnt64-recomp` PC port:
1. In `tnt.recomp.toml`, hook the seed initialization function:
   ```toml
   [[patches.hook]]
   func = "func_800A3550"
   text = "tnt_set_custom_seed(rdram, ctx);"
   ```
2. In your C++ mod callback:
   ```cpp
   extern "C" void tnt_set_custom_seed(uint8_t* rdram, recomp_context* ctx) {
       uint32_t custom_seed = 0x37E11D60; // Paste your seed here
       // Seed address is 0x80129340 (0x80130000 - 0x6CC0)
       MEM_W(0, 0x80129340) = custom_seed;
   }
   ```

---

## License

MIT License. Educational and research tool for speedrun verification and RNG mechanics analysis.
