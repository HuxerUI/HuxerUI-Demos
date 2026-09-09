#include <huxerui/app.h>
#include <cstdio>
#include <cstdlib>
#include <exception>

int main() {
  std::set_terminate([] {
    try { if (const auto error = std::current_exception()) std::rethrow_exception(error); }
    catch (const std::exception& error) { std::fprintf(stderr, "Huxer Drive: %s\n", error.what()); }
    std::_Exit(EXIT_FAILURE);
  });
  return huxerui::RunApplication();
}
