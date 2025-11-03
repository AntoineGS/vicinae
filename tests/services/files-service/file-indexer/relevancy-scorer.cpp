#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "services/files-service/file-indexer/relevancy-scorer.hpp"
#include <filesystem>

TEST_CASE("computeFileTypeMultiplier returns correct weight for documents", FILE_SEARCH_GROUP) {
  RelevancyScorer scorer;

  SECTION("PDF files get high relevance") {
    auto result = scorer.computeFileTypeMultiplier(std::filesystem::path("/home/user/document.pdf"));
    REQUIRE_THAT(result, Catch::Matchers::WithinRel(1.0, 0.001));
  }

  SECTION("Text files get high relevance") {
    auto result = scorer.computeFileTypeMultiplier(std::filesystem::path("/home/user/notes.txt"));
    REQUIRE_THAT(result, Catch::Matchers::WithinRel(1.0, 0.001));
  }

  SECTION("Markdown files get high relevance") {
    auto result = scorer.computeFileTypeMultiplier(std::filesystem::path("/home/user/README.md"));
    REQUIRE_THAT(result, Catch::Matchers::WithinRel(1.0, 0.001));
  }
}

TEST_CASE("computeFileTypeMultiplier returns correct weight for code files", FILE_SEARCH_GROUP) {
  RelevancyScorer scorer;

  SECTION("C++ source files") {
    auto result = scorer.computeFileTypeMultiplier(std::filesystem::path("/project/main.cpp"));
    REQUIRE_THAT(result, Catch::Matchers::WithinRel(0.9, 0.001));
  }

  SECTION("Python files") {
    auto result = scorer.computeFileTypeMultiplier(std::filesystem::path("/project/script.py"));
    REQUIRE_THAT(result, Catch::Matchers::WithinRel(0.9, 0.001));
  }

  SECTION("TypeScript files") {
    auto result = scorer.computeFileTypeMultiplier(std::filesystem::path("/project/app.ts"));
    REQUIRE_THAT(result, Catch::Matchers::WithinRel(0.9, 0.001));
  }
}

TEST_CASE("computeFileTypeMultiplier handles edge cases", FILE_SEARCH_GROUP) {
  RelevancyScorer scorer;

  SECTION("Files without extension get medium weight") {
    auto result = scorer.computeFileTypeMultiplier(std::filesystem::path("/usr/bin/bash"));
    REQUIRE_THAT(result, Catch::Matchers::WithinRel(0.8, 0.001));
  }

  SECTION("Unknown extensions get low weight") {
    auto result = scorer.computeFileTypeMultiplier(std::filesystem::path("/file.xyz123"));
    REQUIRE_THAT(result, Catch::Matchers::WithinRel(0.5, 0.001));
  }

  SECTION("Extensions are case-insensitive") {
    auto result1 = scorer.computeFileTypeMultiplier(std::filesystem::path("/doc.PDF"));
    auto result2 = scorer.computeFileTypeMultiplier(std::filesystem::path("/doc.pdf"));
    REQUIRE_THAT(result1, Catch::Matchers::WithinRel(result2, 0.001));
  }
}
TEST_CASE("computePathDepthMultiplier penalizes deep paths", FILE_SEARCH_GROUP) {
  RelevancyScorer scorer;

  SECTION("Shallow paths (depth <= 3) get no penalty") {
    auto result = scorer.computePathDepthMultiplier(std::filesystem::path("/home/user"));
    REQUIRE_THAT(result, Catch::Matchers::WithinRel(1.0, 0.001));
  }

  SECTION("Medium depth paths (depth 4-6) get small penalty") {
    auto result5 = scorer.computePathDepthMultiplier(std::filesystem::path("/home/user/docs/file"));
    auto result6 = scorer.computePathDepthMultiplier(std::filesystem::path("/home/user/docs/sub/file"));
    auto result7 = scorer.computePathDepthMultiplier(std::filesystem::path("/home/user/docs/sub/deep/file"));

    REQUIRE_THAT(result5, Catch::Matchers::WithinRel(0.90, 0.001));
    REQUIRE_THAT(result6, Catch::Matchers::WithinRel(0.85, 0.001));
    REQUIRE_THAT(result7, Catch::Matchers::WithinRel(0.7, 0.001));
  }

  SECTION("Deep paths (depth > 6) get capped penalty") {
    auto result = scorer.computePathDepthMultiplier(std::filesystem::path("/home/user/a/b/c/d/e/f/g/h/file"));
    REQUIRE_THAT(result, Catch::Matchers::WithinRel(0.7, 0.001));
  }
}

TEST_CASE("computeRecencyMultiplier rewards recent files", FILE_SEARCH_GROUP) {
  RelevancyScorer scorer;
  auto now = std::filesystem::file_time_type::clock::now();

  SECTION("Files modified within last week get high bonus") {
    auto threeDaysAgo = now - std::chrono::days(3);
    auto result = scorer.computeRecencyMultiplier(threeDaysAgo);
    REQUIRE_THAT(result, Catch::Matchers::WithinRel(1.3, 0.001));
  }

  SECTION("Files modified within last month get medium bonus") {
    auto fifteenDaysAgo = now - std::chrono::days(15);
    auto result = scorer.computeRecencyMultiplier(fifteenDaysAgo);
    REQUIRE_THAT(result, Catch::Matchers::WithinRel(1.1, 0.001));
  }

  SECTION("Files modified within last 90 days get no bonus/penalty") {
    auto sixtyDaysAgo = now - std::chrono::days(60);
    auto result = scorer.computeRecencyMultiplier(sixtyDaysAgo);
    REQUIRE_THAT(result, Catch::Matchers::WithinRel(1.0, 0.001));
  }

  SECTION("Files modified within last year get small penalty") {
    auto sixMonthsAgo = now - std::chrono::days(180);
    auto result = scorer.computeRecencyMultiplier(sixMonthsAgo);
    REQUIRE_THAT(result, Catch::Matchers::WithinRel(0.9, 0.001));
  }

  SECTION("Old files get penalty") {
    auto twoYearsAgo = now - std::chrono::days(730);
    auto result = scorer.computeRecencyMultiplier(twoYearsAgo);
    REQUIRE_THAT(result, Catch::Matchers::WithinRel(0.8, 0.001));
  }
}

TEST_CASE("computeScore combines all multipliers correctly", FILE_SEARCH_GROUP) {
  RelevancyScorer scorer;
  auto now = std::filesystem::file_time_type::clock::now();
  auto recentTime = now - std::chrono::days(5);

  SECTION("Recent PDF in shallow path gets high score") {
    auto path = std::filesystem::path("/home/user/document.pdf");
    auto score = scorer.computeScore(path, recentTime);

    // Should combine: location (2.0) * fileType (1.0) * notHidden (1.0) * shallowDepth (1.0) * recent (1.3)
    // Actual computation depends on implementation of home directory check
    REQUIRE(score > 1.0);
  }

  SECTION("Old hidden file in deep path gets low score") {
    auto oldTime = now - std::chrono::days(800);
    auto path = std::filesystem::path("/home/user/.cache/deep/nested/old/file.tmp");
    auto score = scorer.computeScore(path, oldTime);

    // Should get penalties from: hidden file (0.3), deep path, old file (0.8)
    REQUIRE(score < 1.0);
  }

  SECTION("Score has minimum floor of 0.1") {
    auto veryOldTime = now - std::chrono::days(3650);
    auto path = std::filesystem::path("/tmp/.hidden/very/deep/nested/path/file.o");
    auto score = scorer.computeScore(path, veryOldTime);

    REQUIRE(score >= 0.1);
  }

  SECTION("Score computation works without modification time") {
    auto path = std::filesystem::path("/home/user/document.pdf");
    auto score = scorer.computeScore(path, std::nullopt);

    // Should work without recency multiplier
    REQUIRE(score > 0.0);
  }
}
