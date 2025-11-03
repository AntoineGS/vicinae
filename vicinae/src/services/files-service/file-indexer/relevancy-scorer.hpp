#pragma once
#include <filesystem>
#include <optional>

class RelevancyScorer {
public:
  double computeFileTypeMultiplier(const std::filesystem::path &path);
  double computePathDepthMultiplier(const std::filesystem::path &path);
  double computeRecencyMultiplier(const std::filesystem::file_time_type lastModified);
  double computeScore(const std::filesystem::path &path,
                      std::optional<std::filesystem::file_time_type> lastModified);

private:
  double computeLocationMultiplier(const std::filesystem::path &path);
  double computeHiddenFileMultiplier(const std::filesystem::path &path);
};
