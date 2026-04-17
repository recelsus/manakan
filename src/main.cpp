#include "app.hpp"
#include "cli.hpp"
#include <iostream>
#include <utility>

namespace {
constexpr const char* APP_NAME = "manakan";
} // namespace

int main(int argc, char* argv[]) {
  using namespace manakan;

  try {
    auto cli = parse_cli_options(argc, argv);
    if (cli.show_help) {
      print_usage(APP_NAME);
      return 0;
    }
    if (cli.show_version) {
      print_version(APP_NAME);
      return 0;
    }
    return run_command(APP_NAME, std::move(cli));
  } catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << std::endl;
    return 1;
  }
  return 0;
}
