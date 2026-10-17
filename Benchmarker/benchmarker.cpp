#include "Benchmarker/benchmarker.h"

#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace benchmarker {
namespace {

template <typename T>
std::string Number(T value) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::setprecision(std::numeric_limits<double>::max_digits10)
         << value;
  return stream.str();
}

#if defined(_WIN32)
std::string RegistryString(const char* key, const char* name) {
  std::array<char, 512> buffer{};
  DWORD bytes = static_cast<DWORD>(buffer.size());
  const auto error = RegGetValueA(HKEY_LOCAL_MACHINE, key, name, RRF_RT_REG_SZ,
                                  nullptr, buffer.data(), &bytes);
  if (error != ERROR_SUCCESS) {
    return "unavailable (registry error " + std::to_string(error) + ")";
  }
  return buffer.data();
}
#endif

Environment Settings(const ExperimentResult& result) {
  const auto& s = result.simulation_config;
  const auto& c = result.config;
  const auto& p = s.parameters;
  return {{"implementation", result.implementation},
          {"boid_count", Number(s.boid_count)},
          {"dummy_count", "0"},
          {"dummy_change_percent", "0"},
          {"simulation_scope",
           "current Boid-only implementation; no Dummy churn or Archetype"},
          {"reserve_policy",
           result.implementation == "oop"
               ? "Parent pointer vector reserves object count; logical indices "
                 "resize to object count; each object is allocated separately"
           : result.implementation == "ecs"
               ? "Registry/component vectors grow on demand; CachedView "
                 "reserves driver size during query preparation"
               : "unspecified by custom simulation"},
          {"seed", Number(s.random_seed)},
          {"fixed_delta_seconds", Number(c.fixed_delta_seconds)},
          {"warmup_steps", Number(c.warmup_steps)},
          {"measurement_steps", Number(c.measurement_steps)},
          {"spawn_half_extent", Number(s.spawn_half_extent)},
          {"boundary_half_extent", Number(s.boundary_half_extent)},
          {"initial_speed", Number(s.initial_speed)},
          {"neighbor_radius", Number(p.neighbor_radius)},
          {"separation_radius", Number(p.separation_radius)},
          {"separation_weight", Number(p.separation_weight)},
          {"alignment_weight", Number(p.alignment_weight)},
          {"cohesion_weight", Number(p.cohesion_weight)},
          {"preferred_speed", Number(p.preferred_speed)},
          {"max_speed", Number(p.max_speed)},
          {"max_alignment_force", Number(p.max_alignment_force)},
          {"initialization_scope",
           "Initialize(): clear/reserve, RNG and entity/component creation; "
           "excludes constructor"},
          {"process_memory_scope",
           "process lifetime up to post-timing query; includes warmup and "
           "instrumentation"},
          {"storage_scope",
           "simulation root object and owned element storage at final step; "
           "excludes allocator overhead/fragmentation"}};
}

void CsvField(std::ostream& output, std::string_view text) {
  output << '"';
  for (char character : text) {
    if (character == '"') output << '"';
    output << character;
  }
  output << '"';
}

void Row(std::ostream& output, std::string_view kind, std::string_view name,
         std::string_view step, std::string_view value,
         std::string_view status = "available", std::string_view reason = {}) {
  CsvField(output, kind);
  output << ',';
  CsvField(output, name);
  output << ',';
  CsvField(output, step);
  output << ',';
  CsvField(output, value);
  output << ',';
  CsvField(output, status);
  output << ',';
  CsvField(output, reason);
  output << '\n';
}

}  // namespace

void ValidateConfig(const BenchmarkConfig& config) {
  if (!std::isfinite(config.fixed_delta_seconds) ||
      config.fixed_delta_seconds <= 0.0F) {
    throw std::invalid_argument(
        "Benchmark timestep must be finite and positive");
  }
  if (config.measurement_steps == 0U) {
    throw std::invalid_argument("Benchmark measured steps must be positive");
  }
}

Environment CollectEnvironment() {
  Environment environment;
#if defined(_WIN32)
  environment.emplace_back(
      "cpu",
      RegistryString("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                     "ProcessorNameString"));
  constexpr const char* key = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
  // ProductName may retain an older marketing name after a Windows upgrade.
  // Use the build/release values as the OS identity and preserve the raw name.
  environment.emplace_back("os", "Windows");
  environment.emplace_back("os_registry_product_name",
                           RegistryString(key, "ProductName"));
  environment.emplace_back("os_build",
                           RegistryString(key, "CurrentBuildNumber"));
  environment.emplace_back("os_display_version",
                           RegistryString(key, "DisplayVersion"));
  environment.emplace_back("process_id", Number(GetCurrentProcessId()));
#else
  environment.emplace_back(
      "cpu", "unavailable: environment collection supports Windows only");
  environment.emplace_back(
      "os", "unavailable: environment collection supports Windows only");
#endif
  environment.emplace_back("compiler", BENCHMARKER_COMPILER);
  environment.emplace_back("build_flags", BENCHMARKER_BUILD_FLAGS);
  environment.emplace_back("build", BENCHMARKER_BUILD_CONFIG);
  environment.emplace_back("cxx_standard", "20");
  environment.emplace_back("pointer_bits", Number(sizeof(void*) * 8U));
  environment.emplace_back("simulation_threads", "1");
  environment.emplace_back("clock", "std::chrono::steady_clock");
  environment.emplace_back(
      "run_start_unix_ms",
      Number(std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count()));
  std::string run_id = environment.back().second;
#if defined(_WIN32)
  run_id += "-" + Number(GetCurrentProcessId());
#endif
  environment.emplace_back("run_id", std::move(run_id));
  return environment;
}

void PrintSummary(std::ostream& output, const ExperimentResult& result) {
  output << std::setprecision(9) << "Benchmark: " << result.implementation
         << " / naive / simulation-only\n";
  for (const auto& [name, value] : result.environment) {
    output << name << ": " << value << '\n';
  }
  for (const auto& [name, value] : Settings(result)) {
    output << name << ": " << value << '\n';
  }
  output << "Measured steps: " << result.timing.measured_steps
         << "\nInitialization time (ms): " << result.initialization_ms
         << "\nTotal update time (ms): " << result.timing.total_update_ms
         << "\nMean update time (ms/step): " << result.timing.mean_update_ms
         << "\nUpdates/s: " << result.timing.updates_per_second << '\n';
  for (const auto& section : result.timing.sections) {
    output << section.name << " (ms/step): ";
    if (section.recorded_steps == result.timing.measured_steps) {
      output << *section.mean_ms_per_step;
    } else {
      output << "not_measured (recorded " << section.recorded_steps
             << " steps)";
    }
    output << '\n';
  }
  const auto& memory = result.process_memory;
  const auto peak = [&](std::string_view name,
                        const std::optional<std::size_t>& value) {
    output << name << " (bytes): ";
    if (value)
      output << *value;
    else
      output << engine::measurement::StatusName(memory.status) << ": "
             << memory.reason;
    output << '\n';
  };
  peak("Peak Private Commit", memory.peak_private_commit_bytes);
  peak("Peak Working Set", memory.peak_working_set_bytes);
  output << "Container size estimate (bytes"
         << (result.storage.complete() ? "" : ", partial")
         << "): " << result.storage.size_bytes()
         << "\nContainer capacity estimate (bytes"
         << (result.storage.complete() ? "" : ", partial")
         << "): " << result.storage.capacity_bytes() << '\n';
  for (const auto& entry : result.storage.entries()) {
    if (entry.status != engine::measurement::MeasurementStatus::Available) {
      output << entry.name << ": "
             << engine::measurement::StatusName(entry.status) << " / "
             << entry.reason << '\n';
    }
  }
}

void WriteCsv(std::ostream& output, const ExperimentResult& result) {
  output << "kind,name,step,value,status,reason\n";
  Row(output, "metadata", "format_version", {}, "1");
  for (const auto& [name, value] : Settings(result)) {
    Row(output, "setting", name, {}, value);
  }
  for (const auto& [name, value] : result.environment) {
    Row(output, "environment", name, {}, value);
  }
  Row(output, "summary", "initialization_ms", {},
      Number(result.initialization_ms));
  Row(output, "summary", "total_step_ms", {},
      Number(result.timing.total_update_ms));
  Row(output, "summary", "mean_step_ms", {},
      Number(result.timing.mean_update_ms));
  Row(output, "summary", "updates_per_second", {},
      Number(result.timing.updates_per_second));
  for (std::size_t step = 0; step < result.timing.step_ms.size(); ++step) {
    Row(output, "step", "overall_ms", Number(step),
        Number(result.timing.step_ms[step]));
  }
  for (const auto& section : result.timing.sections) {
    const bool complete =
        section.recorded_steps == result.timing.measured_steps;
    Row(output, "section_mean_ms_per_step", section.name, {},
        complete ? Number(*section.mean_ms_per_step) : "",
        complete ? "available" : "not_measured",
        complete ? "" : "Section was not recorded in every measured step");
    for (std::size_t step = 0; step < section.samples.size(); ++step) {
      const auto& sample = section.samples[step];
      const auto status = sample.recorded ? "available" : "not_measured";
      const auto reason = sample.recorded ? "" : "Section was not recorded";
      Row(output, "section_ms", section.name, Number(step),
          sample.recorded ? Number(sample.elapsed_ms) : "", status, reason);
      Row(output, "section_calls", section.name, Number(step),
          sample.recorded ? Number(sample.calls) : "", status, reason);
    }
    if (section.name == "neighbor_calculation") {
      const auto count = result.simulation_config.boid_count;
      const bool valid = complete && count > 1U;
      Row(output, "summary", "neighbor_ms_per_pair", {},
          valid ? Number(*section.mean_ms_per_step /
                         (static_cast<double>(count) *
                          static_cast<double>(count - 1U)))
                : "",
          valid ? "available" : "not_measured",
          valid ? "" : "Needs N > 1 and complete neighbor timings");
    }
  }
  const auto& memory = result.process_memory;
  const auto peak = [&](std::string_view name,
                        const std::optional<std::size_t>& bytes) {
    Row(output, "process_memory_bytes", name, {}, bytes ? Number(*bytes) : "",
        engine::measurement::StatusName(memory.status), memory.reason);
    Row(output, "process_bytes_per_entity", name, {},
        bytes && result.simulation_config.boid_count > 0U
            ? Number(static_cast<double>(*bytes) /
                     static_cast<double>(result.simulation_config.boid_count))
            : "",
        bytes && result.simulation_config.boid_count > 0U ? "available"
                                                          : "not_measured",
        !bytes                                      ? memory.reason
        : result.simulation_config.boid_count == 0U ? "Entity count is zero"
                                                    : "");
  };
  peak("peak_private_commit", memory.peak_private_commit_bytes);
  peak("peak_working_set", memory.peak_working_set_bytes);
  for (const auto& entry : result.storage.entries()) {
    Row(output, "storage_size_bytes", entry.name, {},
        entry.size_bytes ? Number(*entry.size_bytes) : "",
        engine::measurement::StatusName(entry.status), entry.reason);
    Row(output, "storage_capacity_bytes", entry.name, {},
        entry.capacity_bytes ? Number(*entry.capacity_bytes) : "",
        engine::measurement::StatusName(entry.status), entry.reason);
  }
  const bool complete = result.storage.complete();
  Row(output, "summary", "storage_size_bytes", {},
      Number(result.storage.size_bytes()), complete ? "available" : "partial",
      complete ? "" : "Known subtotal; see unavailable entries");
  Row(output, "summary", "storage_capacity_bytes", {},
      Number(result.storage.capacity_bytes()),
      complete ? "available" : "partial",
      complete ? "" : "Known subtotal; see unavailable entries");
  const auto count = result.simulation_config.boid_count;
  if (count > 0U) {
    Row(output, "summary", "storage_size_bytes_per_entity", {},
        Number(static_cast<double>(result.storage.size_bytes()) /
               static_cast<double>(count)),
        complete ? "available" : "partial",
        complete ? "" : "Based on known subtotal");
    Row(output, "summary", "storage_capacity_bytes_per_entity", {},
        Number(static_cast<double>(result.storage.capacity_bytes()) /
               static_cast<double>(count)),
        complete ? "available" : "partial",
        complete ? "" : "Based on known subtotal");
  } else {
    Row(output, "summary", "storage_size_bytes_per_entity", {}, {},
        "not_measured", "Entity count is zero");
    Row(output, "summary", "storage_capacity_bytes_per_entity", {}, {},
        "not_measured", "Entity count is zero");
  }
}

}  // namespace benchmarker
