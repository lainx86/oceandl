#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>

namespace oceandl {

enum class Verbosity {
    Quiet = 0,
    Normal = 1,
    Verbose = 2,
};

class Reporter {
  public:
    Reporter(
        std::ostream& out,
        std::ostream& err,
        Verbosity verbosity = Verbosity::Normal
    );

    bool is_quiet() const;
    bool is_verbose() const;

    void print(const std::string& message, bool force = false) const;
    void blank_line(bool force = false) const;
    void section(
        const std::string& title,
        const std::string& subtitle = {},
        bool force = false
    ) const;
    void field(
        const std::string& label,
        const std::string& value,
        std::size_t label_width = 0,
        bool force = false
    ) const;
    void progress(
        const std::string& label,
        std::uint64_t transferred,
        std::optional<std::uint64_t> total = std::nullopt,
        bool resumed = false
    ) const;
    void finish_progress() const;
    void info(const std::string& message) const;
    void detail(const std::string& message) const;
    void success(const std::string& message) const;
    void warning(const std::string& message) const;
    void error(const std::string& message) const;

  private:
    enum class StreamTarget {
        Out,
        Err,
    };

    enum class Tone {
        Accent,
        Muted,
        Success,
        Warning,
        Error,
        Detail,
    };

    std::string paint(
        const std::string& text,
        Tone tone,
        StreamTarget target,
        bool bold = false
    ) const;
    void clear_live_line() const;
    bool can_render_live_progress() const;

    std::ostream* out_;
    std::ostream* err_;
    Verbosity verbosity_;
    bool out_is_tty_ = false;
    bool out_supports_color_ = false;
    bool err_supports_color_ = false;
    mutable bool live_line_active_ = false;
    mutable std::size_t live_line_width_ = 0;
    mutable std::size_t progress_frame_ = 0;
};

}  // namespace oceandl
