#include "monitor.hpp"

#include <unistd.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// ---- ANSI escape codes ----
constexpr const char* reset   = "\x1b[0m";
constexpr const char* bold    = "\x1b[1m";
constexpr const char* dim     = "\x1b[2m";
constexpr const char* inverse = "\x1b[7m";
constexpr const char* cyan    = "\x1b[36m";
constexpr const char* green   = "\x1b[32m";
constexpr const char* yellow  = "\x1b[33m";
constexpr const char* red     = "\x1b[31m";

constexpr double osc1_hz = 32768.0;

// How much of each new rate sample to blend in (exponential smoothing)
constexpr double smoothing = 0.3;

[[gnu::format(printf, 1, 2)]] std::string fmt(const char* f, ...) {
    char buf[256];
    va_list args;
    va_start(args, f);
    std::vsnprintf(buf, sizeof buf, f, args);
    va_end(args);
    return buf;
}

// Width of a string on screen: skips escape sequences and counts each UTF-8
// code point as one column (all box/arrow characters used here are narrow)
int visible_width(const std::string& s) {
    int w = 0;
    for (std::size_t i = 0; i < s.size(); i++) {
        unsigned char c = s[i];
        if (c == 0x1b) {
            // CSI sequence: ESC [ params... final byte in 0x40-0x7E
            for (i += 2; i < s.size() && !(s[i] >= 0x40 && s[i] <= 0x7E); i++) {}
        } else if ((c & 0xC0) != 0x80) {
            w++;
        }
    }
    return w;
}

std::string repeat(const char* s, int n) {
    std::string out;
    for (int i = 0; i < n; i++) out += s;
    return out;
}

// A titled box of fixed inner width; lines are padded (not truncated) to fit
class Panel {
public:
    Panel(std::string title, int inner_width) : title_(std::move(title)), width_(inner_width) {}

    void add(const std::string& line) { lines_.push_back(line); }

    int height() const { return static_cast<int>(lines_.size()) + 2; }
    int width() const { return width_ + 4; }

    // Render with borders, padding with blank rows to `rows` total lines
    std::vector<std::string> render(int rows = 0) const {
        std::vector<std::string> out;
        std::string top = std::string(dim) + "┌─ " + reset + bold + title_ + reset + dim + " ";
        top += repeat("─", width_ - 1 - visible_width(title_)) + "┐" + reset;
        out.push_back(top);

        int body = std::max(rows - 2, static_cast<int>(lines_.size()));
        for (int i = 0; i < body; i++) {
            std::string text = i < static_cast<int>(lines_.size()) ? lines_[i] : "";
            int pad = std::max(0, width_ - visible_width(text));
            out.push_back(std::string(dim) + "│ " + reset + text + reset +
                          std::string(pad, ' ') + dim + " │" + reset);
        }
        out.push_back(std::string(dim) + "└" + repeat("─", width_ + 2) + "┘" + reset);
        return out;
    }

private:
    std::string title_;
    int width_;
    std::vector<std::string> lines_;
};

// Stack panels vertically into one column
std::vector<std::string> column(std::initializer_list<const Panel*> panels) {
    std::vector<std::string> out;
    for (const Panel* p : panels) {
        auto lines = p->render();
        out.insert(out.end(), lines.begin(), lines.end());
    }
    return out;
}

// Place two columns side by side
std::vector<std::string> side_by_side(const std::vector<std::string>& left, int left_width,
                                      const std::vector<std::string>& right) {
    std::vector<std::string> out;
    std::size_t rows = std::max(left.size(), right.size());
    for (std::size_t i = 0; i < rows; i++) {
        std::string l = i < left.size() ? left[i] : std::string(left_width, ' ');
        std::string r = i < right.size() ? right[i] : "";
        out.push_back(l + " " + r);
    }
    return out;
}

std::string flag(const char* name, bool set) {
    return set ? std::string(inverse) + bold + " " + name + " " + reset
               : std::string(dim) + " " + name + " " + reset;
}

// A horizontal bar of `width` cells filled to `frac` (0..1)
std::string bar(double frac, int width, const char* color) {
    int filled = static_cast<int>(std::clamp(frac, 0.0, 1.0) * width + 0.5);
    return std::string(color) + repeat("█", filled) + reset + dim + repeat("░", width - filled) + reset;
}

// The 16-nibble RAM row containing `step`, with the nibble at `step` highlighted
std::string ram_row(const Cpu& cpu, uint4_t page, std::uint8_t step) {
    const std::array<uint4_t, 16>* row = nullptr;
    uint4_t h = step >> 4;
    switch (page) {
        case 0x0: row = &cpu.ram.page0[h]; break;
        case 0x1: row = &cpu.ram.page1[h]; break;
        case 0x2: if (h < 8) row = &cpu.ram.page2[h]; break;
        case 0xE: if (h < 5 || (h >= 8 && h < 0xD)) row = &cpu.ram.pageE[h]; break;
        case 0xF: if (h < 8) row = &cpu.ram.pageF[h]; break;
    }

    std::string s = fmt("%s%X%X0%s ", dim, page & 0xF, h, reset);
    if (!row) return s + red + "(unmapped)" + reset;

    for (int l = 0; l < 16; l++) {
        bool here = l == (step & 0xF);
        if (here) s += std::string(inverse) + yellow;
        s += fmt("%X", (*row)[l] & 0xF);
        if (here) s += reset;
        if (l != 15) s += ' ';
    }
    return s;
}

std::string format_rate(double hz) {
    if (hz >= 1000.0) return fmt("%6.2f kHz", hz / 1000.0);
    return fmt("%6.0f Hz ", hz);
}

} // namespace

Monitor::Monitor(Cpu& cpu) {
    if (!isatty(STDOUT_FILENO)) {
        throw std::runtime_error("--monitor needs stdout to be a terminal");
    }
    cpu.record_history = true;

    // Read keys one at a time without echo or waiting for Enter. ISIG stays on,
    // so Ctrl-C still raises SIGINT. Without a terminal on stdin, keys are off.
    if (isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &saved_termios_) == 0) {
        termios raw = saved_termios_;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 0;   // read() returns immediately...
        raw.c_cc[VTIME] = 0;  // ...even if no key is waiting
        keys_enabled_ = tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0;
    }

    // Alternate screen, hide cursor, clear
    std::fputs("\x1b[?1049h\x1b[?25l\x1b[2J", stdout);
    std::fflush(stdout);
}

Monitor::~Monitor() {
    if (keys_enabled_) {
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios_);
    }
    // Show cursor, leave the alternate screen
    std::fputs("\x1b[0m\x1b[?25h\x1b[?1049l", stdout);
    std::fflush(stdout);
}

MonitorCommand Monitor::poll_input() {
    if (!keys_enabled_) return MonitorCommand::None;

    MonitorCommand cmd = MonitorCommand::None;
    char buf[32];
    ssize_t n;
    while ((n = read(STDIN_FILENO, buf, sizeof buf)) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            if (buf[i] == ' ' || buf[i] == 'p' || buf[i] == 'P') {
                // Two presses in one poll cancel out
                cmd = cmd == MonitorCommand::None ? MonitorCommand::TogglePause : MonitorCommand::None;
            }
        }
    }
    return cmd;
}

void Monitor::draw(const Cpu& cpu, std::uint64_t ticks_run,
                   std::chrono::duration<double> elapsed, bool paused) {
    // ---- Rates since the last redraw (frozen while paused) ----
    double now = elapsed.count();
    double dt = now - last_elapsed_;
    if (dt > 0.0 && !paused) {
        std::uint64_t dticks = ticks_run - last_ticks_;
        double tick_rate = dticks / dt;
        double instr_rate = (cpu.instr_count - last_instrs_) / dt;
        double halt_frac = dticks ? double(cpu.halt_ticks - last_halt_ticks_) / dticks : 0.0;

        bool first = last_elapsed_ == 0.0;
        double k = first ? 1.0 : smoothing;
        tick_rate_ += k * (tick_rate - tick_rate_);
        instr_rate_ += k * (instr_rate - instr_rate_);
        halt_frac_ += k * (halt_frac - halt_frac_);
    }
    last_ticks_ = ticks_run;
    last_instrs_ = cpu.instr_count;
    last_halt_ticks_ = cpu.halt_ticks;
    last_elapsed_ = now;

    const Registers& r = cpu.reg;
    const ProgramCounter& pc = r.PC;
    constexpr int left_inner = 34;
    constexpr int right_inner = 36;

    // ---- Core: registers, flags, state ----
    Panel core("Core", left_inner);
    core.add(fmt("%sPC%s  %s%d:%X:%02X%s   %sNBP:NPP%s %d:%X",
                 cyan, reset, bold, pc.PCB, pc.PCP & 0xF, pc.PCS, reset,
                 dim, reset, pc.NBP, pc.NPP & 0xF));
    core.add(fmt("%sA%s   %s%X%s       %sB%s   %s%X%s",
                 cyan, reset, bold, r.A & 0xF, reset, cyan, reset, bold, r.B & 0xF, reset));
    core.add(fmt("%sX%s   %X:%02X    %sY%s   %X:%02X",
                 cyan, reset, r.XP & 0xF, r.XHL, cyan, reset, r.YP & 0xF, r.YHL));
    core.add(fmt("%sSP%s  %02X      %sRP%s  %X", cyan, reset, r.SP, cyan, reset, r.RP & 0xF));
    core.add("");
    core.add(std::string(cyan) + "F" + reset + "   " +
             flag("I", cpu.flags.I) + " " + flag("D", cpu.flags.D) + " " +
             flag("Z", cpu.flags.Z) + " " + flag("C", cpu.flags.C) +
             fmt("  %s(%X)%s", dim, cpu.flags.pack(), reset));
    core.add("");
    std::string state;
    if (cpu.halt)                state = std::string(yellow) + bold + "HALT" + reset;
    else if (cpu.clock_wait > 0) state = std::string(cyan) + bold + "WAIT" + reset + fmt(" (%u)", cpu.clock_wait);
    else                         state = std::string(green) + bold + "RUN" + reset;
    core.add(std::string(cyan) + "State " + reset + state);

    // ---- Timing ----
    Panel timing("Timing", left_inner);
    timing.add(fmt("Clock   %s  %s%5.1f%%%s", format_rate(tick_rate_).c_str(),
                   dim, 100.0 * tick_rate_ / osc1_hz, reset));
    timing.add("        " + bar(tick_rate_ / osc1_hz, 24, green));
    timing.add(fmt("Instr   %s", format_rate(instr_rate_).c_str()));
    timing.add(fmt("Halted  %5.1f%%  ", 100.0 * halt_frac_) + bar(halt_frac_, 16, yellow));
    timing.add(fmt("%sRun %.1f s, %llu ticks%s", dim, now,
                   static_cast<unsigned long long>(ticks_run), reset));

    // ---- Stack: nibbles from SP upward (top of stack first) ----
    Panel stack("Stack", left_inner);
    std::string top = fmt("%s%02X%s ", dim, r.SP, reset);
    for (int i = 0; i < 12; i++) {
        std::uint8_t a = r.SP + i;
        top += fmt("%X ", cpu.ram.page0[a >> 4][a & 0xF] & 0xF);
    }
    stack.add(top);
    stack.add(std::string(dim) + "   ↑ top" + reset);

    // ---- Recent instructions, oldest first ----
    Panel trace("Recent instructions", right_inner);
    constexpr std::size_t shown = 20;
    for (std::size_t i = Cpu::history_size - shown; i < Cpu::history_size; i++) {
        const InstrRecord& rec = cpu.history[(cpu.history_pos + i) % Cpu::history_size];
        bool newest = i == Cpu::history_size - 1;
        if (rec.text[0] == '\0') { trace.add(""); continue; }
        std::string line = fmt("%s%d:%X:%02X%s  %s%03X%s  ",
                               dim, rec.bank, rec.page & 0xF, rec.step, reset,
                               dim, rec.opcode & 0xFFF, reset);
        line += newest ? std::string(bold) + "▶ " + rec.text.data() + reset
                       : std::string("  ") + rec.text.data();
        trace.add(line);
    }

    // ---- Memory at the index registers ----
    Panel mem("Memory at MX / MY", left_inner + right_inner + 5);
    mem.add(fmt("%sMX%s  ", cyan, reset) + ram_row(cpu, r.XP & 0xF, r.XHL));
    mem.add(fmt("%sMY%s  ", cyan, reset) + ram_row(cpu, r.YP & 0xF, r.YHL));

    // ---- Compose ----
    std::vector<std::string> left = column({&core, &timing, &stack});
    std::vector<std::string> right = trace.render(static_cast<int>(left.size()));
    std::vector<std::string> screen = side_by_side(left, core.width(), right);
    auto bottom = mem.render();
    screen.insert(screen.end(), bottom.begin(), bottom.end());

    std::string out = "\x1b[H";  // cursor home; overwrite in place to avoid flicker
    out += std::string(bold) + " tama62 core monitor " + reset;
    out += paused ? std::string(inverse) + yellow + bold + " PAUSED " + reset
                  : std::string(inverse) + green + bold + " RUNNING " + reset;
    out += std::string(dim) + (keys_enabled_ ? "   Space: pause/resume" : "") +
           "   Ctrl-C: quit" + reset + "\x1b[K\n";
    for (const std::string& line : screen) {
        out += line + "\x1b[K\n";
    }
    out += "\x1b[J";  // clear anything below (e.g. stray log output)
    std::fwrite(out.data(), 1, out.size(), stdout);
    std::fflush(stdout);
}
