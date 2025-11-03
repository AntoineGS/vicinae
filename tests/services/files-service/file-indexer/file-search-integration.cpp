#include <catch2/catch_test_macros.hpp>
#include "services/files-service/file-indexer/file-indexer-db.hpp"
#include "services/files-service/file-indexer/file-indexer.hpp"
#include "test-helpers.hpp"
#include <filesystem>
#include <fstream>

TEST_CASE("Single word prefix search finds matching files", FILE_SEARCH_GROUP) {
  // Create temporary directory and files for testing
  auto tempDir = std::filesystem::temp_directory_path() / "vicinae-test";
  std::filesystem::create_directories(tempDir);

  auto docPath = tempDir / "document.pdf";
  auto docsPath = tempDir / "docs";
  auto picturesPath = tempDir / "pictures";

  std::ofstream(docPath).close(); // Create empty file
  std::filesystem::create_directory(docsPath);
  std::filesystem::create_directory(picturesPath);

  FileIndexerDatabase db(":memory:");
  db.runMigrations();

  db.indexFiles({docPath, docsPath, picturesPath});

  FileIndexer indexer(std::ref(db));

  auto results = waitForSearchResults(indexer.queryAsync("doc"));

  // Cleanup
  std::filesystem::remove_all(tempDir);

  REQUIRE(results.size() == 2);

  bool foundDocument = false;
  bool foundDocs = false;

  for (const auto &result : results) {
    if (result.path == docPath) foundDocument = true;
    if (result.path == docsPath) foundDocs = true;
  }

  REQUIRE(foundDocument);
  REQUIRE(foundDocs);
}
