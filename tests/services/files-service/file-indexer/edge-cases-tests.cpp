#include <catch2/catch_test_macros.hpp>
#include "services/files-service/file-indexer/file-indexer-db.hpp"
#include "services/files-service/file-indexer/file-indexer.hpp"
#include "test-helpers.hpp"
#include <filesystem>
#include <fstream>

TEST_CASE("Empty database returns no results", FILE_SEARCH_GROUP) {
  FileIndexerDatabase db(":memory:");
  db.runMigrations();

  FileIndexer indexer(std::ref(db));
  auto results = waitForSearchResults(indexer.queryAsync("anything"));

  REQUIRE(results.empty());
}

TEST_CASE("Database with files but query has no matches", FILE_SEARCH_GROUP) {
  auto tempDir = std::filesystem::temp_directory_path() / "vicinae-test-nomatch";
  std::filesystem::create_directories(tempDir);

  auto path = tempDir / "document.pdf";
  std::ofstream(path).close();

  FileIndexerDatabase db(":memory:");
  db.runMigrations();
  db.indexFiles({path});

  FileIndexer indexer(std::ref(db));
  auto results = waitForSearchResults(indexer.queryAsync("xyz123nonexistent"));

  std::filesystem::remove_all(tempDir);

  REQUIRE(results.empty());
}

TEST_CASE("Very long query is handled gracefully", FILE_SEARCH_GROUP) {
  auto tempDir = std::filesystem::temp_directory_path() / "vicinae-test-longquery";
  std::filesystem::create_directories(tempDir);

  auto path = tempDir / "test.txt";
  std::ofstream(path).close();

  FileIndexerDatabase db(":memory:");
  db.runMigrations();
  db.indexFiles({path});

  FileIndexer indexer(std::ref(db));

  // Create a very long query (1000 characters)
  std::string longQuery(1000, 'a');
  auto results = waitForSearchResults(indexer.queryAsync(longQuery));

  std::filesystem::remove_all(tempDir);

  // Should not crash, results may be empty
  REQUIRE(results.empty());
}

TEST_CASE("Query with only special characters returns no results", FILE_SEARCH_GROUP) {
  auto tempDir = std::filesystem::temp_directory_path() / "vicinae-test-special";
  std::filesystem::create_directories(tempDir);

  auto path = tempDir / "test.txt";
  std::ofstream(path).close();

  FileIndexerDatabase db(":memory:");
  db.runMigrations();
  db.indexFiles({path});

  FileIndexer indexer(std::ref(db));
  auto results = waitForSearchResults(indexer.queryAsync("!@#$%^&*()"));

  std::filesystem::remove_all(tempDir);

  // Special characters should be handled gracefully
  REQUIRE(results.empty());
}

TEST_CASE("Multiple sequential queries on same database work correctly", FILE_SEARCH_GROUP) {
  auto tempDir = std::filesystem::temp_directory_path() / "vicinae-test-sequential";
  std::filesystem::create_directories(tempDir);

  auto path1 = tempDir / "document.pdf";
  auto path2 = tempDir / "test.txt";

  std::ofstream(path1).close();
  std::ofstream(path2).close();

  FileIndexerDatabase db(":memory:");
  db.runMigrations();
  db.indexFiles({path1, path2});

  FileIndexer indexer(std::ref(db));

  // Run multiple sequential queries
  auto results1 = waitForSearchResults(indexer.queryAsync("document"));
  auto results2 = waitForSearchResults(indexer.queryAsync("test"));
  auto results3 = waitForSearchResults(indexer.queryAsync("pdf"));

  std::filesystem::remove_all(tempDir);

  // All queries should complete successfully
  REQUIRE(results1[0].path == path1);
  REQUIRE(results2[0].path == path2);
  REQUIRE(results3[0].path == path1);
}
