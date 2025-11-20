#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <csignal>
#include <unistd.h>
#include <unordered_map>

// ---------------- Terminal helpers ---------------- //

namespace terminal {
    static volatile sig_atomic_t tty = 0; // stdout is a terminal: 1 = yes, 0 = no

    inline void hide()  { if (tty) write(STDOUT_FILENO, "\x1b[?25l", 6); }
    inline void show()  { if (tty) write(STDOUT_FILENO, "\x1b[?25h", 6); }
    inline void clear() { if (tty) write(STDOUT_FILENO, "\x1b[2J",   4); }
    inline void home()  { if (tty) write(STDOUT_FILENO, "\x1b[H",    3); }

    inline void on_signal(int sig) {
        show();                    // show cursor on program escape signals
        _exit(128 + (sig & 0x7F)); // conventional exit code: 128 + signal number
    }

    inline void setup() {
        tty = isatty(STDOUT_FILENO) ? 1 : 0; // detect once for async-signal-safe use

        std::signal(SIGINT,  on_signal); // interrupt signal (Ctrl+C)
        std::signal(SIGTERM, on_signal); // terminate signal ('kill <pid>')
        std::signal(SIGHUP,  on_signal); // hang up signal (terminal is closed)

        std::atexit([]{ show(); });  // show cursor on normal program exit

        hide();  // hide cursor
        clear(); // clear the screen
        home();  // move cursor to top-left
    }
}

// ---------------- Argument parsing ---------------- //

struct Args {
    int rows = 24, cols = 40, fps = 15;
    std::optional<std::uint32_t> seed;
    std::vector<std::string> patterns;
};

static bool starts_with(std::string_view s, std::string_view target) {
    return s.rfind(target, 0) == 0;
}

static std::pair<std::string_view,std::string_view> parse_flag_or_exit(std::string_view arg) {
    size_t eq = arg.find('=');

    bool missing_prefix  = !starts_with(arg, "--");        // missing "--" prefix
    bool no_equal        = (eq == std::string_view::npos); // missing "=" sign
    bool empty_name      = (eq <= 2);                      // "--" takes positions 0 and 1; name must start at position 2
    bool empty_value     = (eq + 1 >= arg.size());         // nothing after "="

    if (missing_prefix || no_equal || empty_name || empty_value) {
        std::cerr << "Error: expected --name=value, got \"" << arg << "\"\n";
        std::exit(1);
    }

    auto name  = arg.substr(2, eq - 2);
    auto value = arg.substr(eq + 1);
    return {name, value};
}

static int to_int_or_exit(std::string_view arg) {
    auto [name, val] = parse_flag_or_exit(arg);
    try {
        return std::stoi(std::string(val));
    } catch (...) {
        std::cerr << "Error: " << name << " must be an integer, got \"" << val << "\"\n";
        std::exit(1);
    }
}

static std::uint32_t to_u32_or_exit(std::string_view arg) {
    auto [name, val] = parse_flag_or_exit(arg);
    try {
        return static_cast<std::uint32_t>(std::stoul(std::string(val)));
    } catch (...) {
        std::cerr << "Error: " << name << " must be an unsigned integer, got \"" << val << "\"\n";
        std::exit(1);
    }
}

static std::vector<std::string> to_string_list_or_exit(std::string_view arg) {
    auto [name, val] = parse_flag_or_exit(arg);
    std::vector<std::string> out;
    std::stringstream ss{std::string(val)};
    std::string item;

    while (std::getline(ss, item, ',')) {
        if (!item.empty()) out.push_back(item);
    }

    if (out.empty()) {
        std::cerr << "Error: " << name << " requires at least one value\n";
        std::exit(1);
    }

    return out;
}

static Args parse_args(int argc, char** argv) {
    Args a; // defaults already set in struct

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        if      (starts_with(arg, "--rows"))    a.rows = to_int_or_exit(arg);
        else if (starts_with(arg, "--cols"))    a.cols = to_int_or_exit(arg);
        else if (starts_with(arg, "--fps"))     a.fps = to_int_or_exit(arg);
        else if (starts_with(arg, "--seed"))    a.seed = to_u32_or_exit(arg);
        else if (starts_with(arg, "--patterns")) a.patterns = to_string_list_or_exit(arg);
        else {
            std::cerr << "Unknown argument: " << arg << "\n";
            std::exit(1);
        }
    }

    return a;
}

// ---------------- Game of Life simulation ---------------- //

struct Board {
    using Cell = bool; // false = dead, true = alive
    int height, width;
    std::vector<Cell> cells;

    Board(int h, int w) : height(h), width(w), cells(h * w, false) {}

    int index(int r, int c) const { return r * width + c; }
    int wrap(int x, int max) const { return (x + max) % max; }

    Cell get(int r, int c) const {
        return cells[index(wrap(r, height), wrap(c, width))];
    }

    void set(int r, int c, Cell v) {
        cells[index(wrap(r, height), wrap(c, width))] = v;
    }

    int count_neighbors(int r, int c) const {
        int n = 0;
        for (int dr = -1; dr <= 1; ++dr)
            for (int dc = -1; dc <= 1; ++dc)
                if (dr || dc) 
                    n += static_cast<int>(get(r + dr, c + dc));
        return n;
    }

    Board advance() const {
        Board next(height, width);
        for (int r = 0; r < height; ++r)
            for (int c = 0; c < width; ++c) {
                bool alive = get(r, c);
                int n = count_neighbors(r, c);
                bool stay_alive = alive && (n == 2 || n == 3);
                bool become_alive = !alive && (n == 3);
                next.set(r, c, stay_alive || become_alive);
            }
        return next;
    }

    void render(std::ostream& os) const {
        terminal::home(); // move cursor to top-left
        for (int r = 0; r < height; ++r) {
            for (int c = 0; c < width; ++c)
                os << (get(r, c) ? "██" : "  ");
            os << '\n';
        }
    }
};

static void load_patterns(Board& b, const std::vector<std::string>& names) {
    static const std::unordered_map<std::string, std::vector<std::string>> patterns = {
        // Still Lifes
        {"block", {
            "OO",
            "OO"
        }},
        {"beehive", {
            ".OO.",
            "O..O",
            ".OO."
        }},
        {"loaf", {
            ".OO.",
            "O..O",
            ".O.O",
            "..O."
        }},
        {"boat", {
            "OO.",
            "O.O",
            ".O."
        }},
        {"tub", {
            ".O.",
            "O.O",
            ".O."
        }},

        // Oscillators
        {"blinker", {
            "...",
            "OOO",
            "...",
        }},
        {"toad", {
            "....",
            ".OOO",
            "OOO.",
            "....",
        }},
        {"beacon", {
            "OO..",
            "OO..",
            "..OO",
            "..OO"
        }},
        {"pulsar", {
            "....O.....O....",
            "....O.....O....",
            "....OO...OO....",
            "...............",
            "OOO..OO.OO..OOO",
            "..O.O.O.O.O.O..",
            "....OO...OO....",
            "...............",
            "....OO...OO....",
            "..O.O.O.O.O.O..",
            "OOO..OO.OO..OOO",
            "...............",
            "....OO...OO....",
            "....O.....O....",
            "....O.....O....",
        }},
        {"pentadecathlon", {
            ".........",
            ".........",
            "...OOO...",
            "....O....",
            "....O....",
            "...OOO...",
            ".........",
            "...OOO...",
            "...OOO...",
            ".........",
            "...OOO...",
            "....O....",
            "....O....",
            "...OOO...",
            ".........",
            ".........",
        }},

        // Spaceships
        {"glider", {
            ".O.",
            "..O",
            "OOO"
        }},
        {"lwss", {
            "O..O.",
            "....O",
            "O...O",
            ".OOOO"
        }},
        {"mwss", {
            "..O...",
            "O...O.",
            ".....O",
            "O....O",
            ".OOOOO"
        }},
        {"hwss", {
            "..OO...",
            "O....O.",
            "......O",
            "O.....O",
            ".OOOOOO"
        }},
    };

    std::fill(b.cells.begin(), b.cells.end(), false);

    int margin = 2;          // 2-cell margin
    int cur_row = margin;    // initialize with top margin
    int cur_col = margin;    // initialize with left margin
    int line_height = 0;     // tallest pattern in this row

    for (const auto& s : names) {
        auto it = patterns.find(s);
        if (it == patterns.end()) {
            std::cerr << "Unknown pattern: " << s << "\n";
            std::exit(1);
        }

        const auto& pattern = it->second;
        if (pattern.empty()) 
            continue;

        int pattern_height = static_cast<int>(pattern.size());
        int pattern_width  = static_cast<int>(pattern[0].size());

        // If pattern too wide or tall to fit on board
        if (pattern_width + 2 > b.width || pattern_height > b.height) {
            std::cerr << "Pattern \"" << s << "\" too large to fit on " << b.height << "x" << b.width << " board\n";
            continue;
        }

        // If pattern too wide to fit on current row
        if (cur_col + pattern_width + margin > b.width) {
            cur_row += line_height + margin;   // move down one row of patterns
            cur_col = margin;                  // reset to left margin
            line_height = 0;
        }

        // If no vertical room left
        if (cur_row + pattern_height > b.height) {
            std::cerr << "No more room to place pattern \"" << s << "\"; remaining patterns skipped.\n";
            break;
        }

        // Draw pattern
        for (int r = 0; r < pattern_height; ++r) {
            const std::string& pattern_row = pattern[r];
            for (int c = 0; c < pattern_width; ++c) {
                if (pattern_row[c] == 'O') {
                    b.set(cur_row + r, cur_col + c, true);
                }
            }
        }

        // Update layout
        line_height = std::max(line_height, pattern_height);
        cur_col += pattern_width + margin;
    }
}

// ---------------- main ---------------- //

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    terminal::setup();

    Args args = parse_args(argc, argv);
    Board board(args.rows, args.cols);

    if (!args.patterns.empty()) {
        load_patterns(board, args.patterns);
    } else {
        std::mt19937 rng(args.seed ? *args.seed : std::random_device{}());
        std::bernoulli_distribution alive(0.50);
        for (int r = 0; r < args.rows; ++r)
            for (int c = 0; c < args.cols; ++c)
                board.set(r, c, alive(rng));
    }

    using clock = std::chrono::steady_clock;
    auto frame = std::chrono::milliseconds(1000 / args.fps);
    auto next_tick = clock::now();

    while (true) {
        board.render(std::cout);

        next_tick += frame;
        std::this_thread::sleep_until(next_tick);

        Board next = board.advance();
        if (next.cells == board.cells) 
            break;   // reached a stable state

        board = std::move(next);
    }

    return 0;
}
