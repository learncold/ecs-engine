#pragma once

#include <filesystem>

namespace benchmarker {
struct ExperimentResult;
}

namespace app {

// Publish only after serialization and close succeed. Existing results survive
// any failure before the final same-directory rename.
void WriteBenchmarkFile(const std::filesystem::path& path,
                        const benchmarker::ExperimentResult& result);

}  // namespace app
