#include "oceandl/reporter.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <ostream>
#include <string_view>

#include <fmt/format.h>

#include "oceandl/utils.hpp"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace oceandl {

namespace {

bool is_env_enabled(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && std::string_view(value) != "0";
}

bool stream_is_tty(std::ostream* stream) {
    if (stream == nullptr) {
        return false;
    }

#ifdef _WIN32
    if (stream == &std::cout) {
        return _isatty(_fileno(stdout)) != 0;
    }
    if (stream == &std::cerr) {
        return _isatty(_fileno(stderr)) != 0;
    }
#else
    if (stream == &std::cout) {
        return ::isatty(STDOUT_FILENO) != 0;
    }
    if (stream == &std::cerr) {
        return ::isatty(STDERR_FILENO) != 0;
    }
#endif

    return false;
}

bool stream_supports_color(std::ostream* stream) {
    if (std::getenv("NO_COLOR") != nullptr) {
        return false;
    }
    if (is_env_enabled("CLICOLOR_FORCE")) {
        return true;
    }

    const char* term = std::getenv("TERM");
    if (term == nullptr || std::string_view(term).empty() || std::string_view(term) == "dumb") {
        return false;
    }

    return stream_is_tty(stream);
}

}  // namespace

Reporter::Reporter(std::ostream& out, std::ostream& err, Verbosity verbosity)
    : out_(&out),
      err_(&err),
      verbosity_(verbosity),
      out_is_tty_(stream_is_tty(out_)),
      out_supports_color_(stream_supports_color(out_)),
      err_supports_color_(stream_supports_color(err_)) {}

bool Reporter::is_quiet() const {
    return verbosity_ == Verbosity::Quiet;
}

bool Reporter::is_verbose() const {
    return verbosity_ == Verbosity::Verbose;
}

void Reporter::print(const std::string& message, bool force) const {
    if (force || !is_quiet()) {
        clear_live_line();
        (*out_) << message << '\n' << std::flush;
    }
}

void Reporter::blank_line(bool force) const {
    if (force || !is_quiet()) {
        clear_live_line();
        (*out_) << '\n' << std::flush;
    }
}

void Reporter::section(const std::string& title, const std::string& subtitle, bool force) const {
    if (!(force || !is_quiet())) {
        return;
    }

    clear_live_line();
    (*out_) << paint(fmt::format("== {} ==", title), Tone::Accent, StreamTarget::Out, true)
            << '\n';
    if (!subtitle.empty()) {
        (*out_) << paint(subtitle, Tone::Muted, StreamTarget::Out) << '\n';
    }
    (*out_) << std::flush;
}

void Reporter::field(
    const std::string& label,
    const std::string& value,
    std::size_t label_width,
    bool force
) const {
    if (!(force || !is_quiet())) {
        return;
    }

    if (label_width == 0) {
        label_width = label.size();
    }

    clear_live_line();
    const auto padded_label = fmt::format("{:<{}}", label, label_width);
    (*out_) << "    " << paint(padded_label, Tone::Muted, StreamTarget::Out) << "  " << value
            << '\n' << std::flush;
}

void Reporter::progress(
    const std::string& label,
    std::uint64_t transferred,
    std::optional<std::uint64_t> total,
    bool resumed
) const {
    if (!can_render_live_progress()) {
        return;
    }

    std::string line;
    const auto decorated_label = resumed ? fmt::format("{} [resume]", label) : label;
    if (total.has_value() && *total > 0) {
        const auto completed = std::min(transferred, *total);
        constexpr std::size_t kBarWidth = 24;
        const auto ratio = static_cast<double>(completed) / static_cast<double>(*total);
        const auto filled = static_cast<std::size_t>(ratio * static_cast<double>(kBarWidth));
        const auto clamped_filled = std::min(filled, kBarWidth);
        const auto percentage = static_cast<int>(ratio * 100.0 + 0.5);
        line = fmt::format(
            "[dl] [{}{}] {:>3}%  {} / {}  {}",
            std::string(clamped_filled, '#'),
            std::string(kBarWidth - clamped_filled, '-'),
            std::min(percentage, 100),
            format_bytes(completed),
            format_bytes(*total),
            decorated_label
        );
    } else {
        static constexpr std::string_view kFrames[] = {"|", "/", "-", "\\"};
        line = fmt::format(
            "[dl] [{}] {}  {}",
            kFrames[progress_frame_ % std::size(kFrames)],
            format_bytes(transferred),
            decorated_label
        );
        ++progress_frame_;
    }

    const auto padded_width = live_line_active_ ? std::max(live_line_width_, line.size()) : line.size();
    (*out_) << '\r' << line;
    if (padded_width > line.size()) {
        (*out_) << std::string(padded_width - line.size(), ' ');
    }
    (*out_) << std::flush;

    live_line_active_ = true;
    live_line_width_ = padded_width;
}

void Reporter::finish_progress() const {
    clear_live_line();
}

void Reporter::info(const std::string& message) const {
    if (!is_quiet()) {
        clear_live_line();
        (*out_) << paint("[info]", Tone::Accent, StreamTarget::Out, true) << ' ' << message
                << '\n' << std::flush;
    }
}

void Reporter::detail(const std::string& message) const {
    if (is_verbose()) {
        clear_live_line();
        (*out_) << "  " << paint("[debug]", Tone::Detail, StreamTarget::Out, true) << ' '
                << message << '\n' << std::flush;
    }
}

void Reporter::success(const std::string& message) const {
    if (!is_quiet()) {
        clear_live_line();
        (*out_) << paint("[ok]", Tone::Success, StreamTarget::Out, true) << ' ' << message
                << '\n' << std::flush;
    }
}

void Reporter::warning(const std::string& message) const {
    if (!is_quiet()) {
        clear_live_line();
        (*out_) << paint("[warn]", Tone::Warning, StreamTarget::Out, true) << ' ' << message
                << '\n' << std::flush;
    }
}

void Reporter::error(const std::string& message) const {
    clear_live_line();
    (*err_) << paint("[err]", Tone::Error, StreamTarget::Err, true) << ' ' << message << '\n'
            << std::flush;
}

std::string Reporter::paint(
    const std::string& text,
    Tone tone,
    StreamTarget target,
    bool bold
) const {
    const bool supports_color =
        target == StreamTarget::Out ? out_supports_color_ : err_supports_color_;
    if (!supports_color) {
        return text;
    }

    std::string code;
    switch (tone) {
        case Tone::Accent:
            code = bold ? "1;36" : "36";
            break;
        case Tone::Muted:
            code = bold ? "1;2;37" : "2";
            break;
        case Tone::Success:
            code = bold ? "1;32" : "32";
            break;
        case Tone::Warning:
            code = bold ? "1;33" : "33";
            break;
        case Tone::Error:
            code = bold ? "1;31" : "31";
            break;
        case Tone::Detail:
            code = bold ? "1;90" : "90";
            break;
    }

    return fmt::format("\033[{}m{}\033[0m", code, text);
}

void Reporter::clear_live_line() const {
    if (!live_line_active_) {
        return;
    }

    (*out_) << '\r' << std::string(live_line_width_, ' ') << '\r' << std::flush;
    live_line_active_ = false;
    live_line_width_ = 0;
}

bool Reporter::can_render_live_progress() const {
    const char* term = std::getenv("TERM");
    return !is_quiet()
        && out_is_tty_
        && term != nullptr
        && std::string_view(term) != "dumb";
}

}  // namespace oceandl
