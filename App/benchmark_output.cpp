#include "App/benchmark_output.h"

#include <chrono>
#include <fstream>
#include <stdexcept>
#include <system_error>

#include "Benchmarker/benchmarker.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace app {

void WriteBenchmarkFile(const std::filesystem::path& path,
                        const benchmarker::ExperimentResult& result) {
  if (path.empty())
    throw std::invalid_argument("Output path must not be empty");
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path());
  }
  auto temporary = path;
  temporary += ".tmp-" +
               std::to_string(
                   std::chrono::steady_clock::now().time_since_epoch().count());
  // Do not truncate another pending output, even in the unlikely case of a
  // temporary-name collision. Benchmark processes themselves run serially.
  if (std::filesystem::exists(temporary)) {
    throw std::runtime_error("Benchmark temporary output already exists");
  }
  try {
    std::ofstream output(temporary, std::ios::binary);
    output.exceptions(std::ios::failbit | std::ios::badbit);
    benchmarker::WriteCsv(output, result);
    output.close();
#if defined(_WIN32)
    if (!MoveFileExW(temporary.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING)) {
      throw std::system_error(static_cast<int>(GetLastError()),
                              std::system_category(),
                              "Cannot publish benchmark output");
    }
#else
    std::filesystem::rename(temporary, path);
#endif
  } catch (...) {
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    throw;
  }
}

}  // namespace app
