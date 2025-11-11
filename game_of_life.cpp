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

// ------------------ ANSI helpers ------------------ //

namespace ansi {
    inline void hide_cursor(std::ostream& os) { os << "\x1b[?25l"; }
    inline void show_cursor(std::ostream& os) { os << "\x1b[?25h"; }
    inline void clear_screen(std::ostream& os) { os << "\x1b[2J"; }
    inline void home(std::ostream& os) { os << "\x1b[H"; }
}

struct CursorGuard {
    std::ostream& os;
    explicit CursorGuard(std::ostream& o) : os(o) { ansi::hide_cursor(os); }
    ~CursorGuard() { ansi::show_cursor(os); }
};

// ---------------- Argument parsing ---------------- //

struct Args {
    int rows = 24, cols = 40, fps = 15;
    std::optional<std::uint32_t> seed;
    std::vector<std::string> presets;
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
        else if (starts_with(arg, "--presets")) a.presets = to_string_list_or_exit(arg);
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
        ansi::home(os);
        for (int r = 0; r < height; ++r) {
            for (int c = 0; c < width; ++c)
                os << (get(r, c) ? "██" : "  ");
            os << '\n';
        }
        os.flush();
    }
};

static void seed_presets(Board& b, const std::vector<std::string>& names) {
    for (auto& s : names) {
        if (s == "glider") {
            int r = 2, c = 2;
            b.set(r+0, c+1, 1); b.set(r+1, c+2, 1);
            b.set(r+2, c+0, 1); b.set(r+2, c+1, 1); b.set(r+2, c+2, 1);
        }
        else if (s == "exploder") {
            int r = 10, c = 10;
            b.set(r, c, 1);
            b.set(r, c-1, 1); b.set(r, c+1, 1);
            b.set(r-1, c, 1); b.set(r+1, c, 1);
            b.set(r-2, c, 1); b.set(r+2, c, 1);
        }
        else {
            std::cerr << "Unknown preset: " << s << "\n";
            std::exit(1);
        }
    }
}

// ---------------- main ---------------- //

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Args args = parse_args(argc, argv);
    Board board(args.rows, args.cols);

    if (!args.presets.empty()) {
        seed_presets(board, args.presets);
    } else {
        std::mt19937 rng(args.seed ? *args.seed : std::random_device{}());
        std::bernoulli_distribution alive(0.20);
        for (int r = 0; r < args.rows; ++r)
            for (int c = 0; c < args.cols; ++c)
                board.set(r, c, alive(rng));
    }

    CursorGuard guard(std::cout);
    ansi::clear_screen(std::cout);

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
