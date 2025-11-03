#include <catch2/catch_test_macros.hpp>
#include "services/files-service/file-indexer/file-indexer-db.hpp"
#include "services/files-service/file-indexer/file-indexer.hpp"
#include "test-helpers.hpp"
#include <filesystem>
#include <fstream>

// Helper to create test files
auto createTestFiles(const std::filesystem::path &tempDir, const std::vector<std::string> &names) {
  std::filesystem::create_directories(tempDir);
  std::vector<std::filesystem::path> paths;
  for (const auto &name : names) {
    auto path = tempDir / name;
    if (name.back() == '/') {
      std::filesystem::create_directory(path);
    } else {
      std::ofstream(path).close();
    }
    paths.push_back(path);
  }
  return paths;
}

TEST_CASE("Multi-word queries find best matches", FILE_SEARCH_GROUP) {
  auto tempDir = std::filesystem::temp_directory_path() / "vicinae-test-multi";
  auto paths = createTestFiles(tempDir, {"project-readme.md", "project.txt", "readme-old.md"});

  FileIndexerDatabase db(":memory:");
  db.runMigrations();
  db.indexFiles(paths);

  FileIndexer indexer(std::ref(db));
  auto results = waitForSearchResults(indexer.queryAsync("project readme"));

  std::filesystem::remove_all(tempDir);

  REQUIRE(!results.empty());
  REQUIRE(results[0].path == paths[0]);
}

TEST_CASE("Queries with special characters are handled correctly", FILE_SEARCH_GROUP) {
  auto tempDir = std::filesystem::temp_directory_path() / "vicinae-test-special";
  auto paths = createTestFiles(tempDir, {"config.json", "config-backup.json"});

  FileIndexerDatabase db(":memory:");
  db.runMigrations();
  db.indexFiles(paths);

  FileIndexer indexer(std::ref(db));
  auto results = waitForSearchResults(indexer.queryAsync("config.json"));

  std::filesystem::remove_all(tempDir);

  REQUIRE(results.size() >= 1);
  REQUIRE(results[0].path == paths[0]);
}

TEST_CASE("Case insensitive search works correctly", FILE_SEARCH_GROUP) {
  auto tempDir = std::filesystem::temp_directory_path() / "vicinae-test-case";
  auto paths = createTestFiles(tempDir, {"readme.md", "ReadMe.txt", "README.markdown"});

  FileIndexerDatabase db(":memory:");
  db.runMigrations();
  db.indexFiles(paths);

  FileIndexer indexer(std::ref(db));
  auto results = waitForSearchResults(indexer.queryAsync("README"));

  std::filesystem::remove_all(tempDir);

  REQUIRE(results.size() == 3);
}

TEST_CASE("Partial filename matching respects boundaries", FILE_SEARCH_GROUP) {
  auto tempDir = std::filesystem::temp_directory_path() / "vicinae-test-partial";
  auto paths = createTestFiles(tempDir, {"main.cpp", "maintain.cpp", "domain.cpp"});

  FileIndexerDatabase db(":memory:");
  db.runMigrations();
  db.indexFiles(paths);

  FileIndexer indexer(std::ref(db));
  auto results = waitForSearchResults(indexer.queryAsync("main"));

  std::filesystem::remove_all(tempDir);

  REQUIRE(results.size() == 2);

  bool foundMain = false;
  bool foundMaintain = false;
  bool foundDomain = false;

  for (const auto &result : results) {
    if (result.path.filename() == "main.cpp") foundMain = true;
    if (result.path.filename() == "maintain.cpp") foundMaintain = true;
    if (result.path.filename() == "domain.cpp") foundDomain = true;
  }

  REQUIRE(foundMain);
  REQUIRE(foundMaintain);
  REQUIRE(!foundDomain);
}

TEST_CASE("No results for non-existent files", FILE_SEARCH_GROUP) {
  auto tempDir = std::filesystem::temp_directory_path() / "vicinae-test-noresults";
  auto paths = createTestFiles(tempDir, {"document.pdf", "notes.txt"});

  FileIndexerDatabase db(":memory:");
  db.runMigrations();
  db.indexFiles(paths);

  FileIndexer indexer(std::ref(db));
  auto results = waitForSearchResults(indexer.queryAsync("nonexistent"));

  std::filesystem::remove_all(tempDir);

  REQUIRE(results.empty());
}
