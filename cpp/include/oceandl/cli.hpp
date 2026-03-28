#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace oceandl {

class CliApp {
  public:
    int run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) const;
};

}  // namespace oceandl
