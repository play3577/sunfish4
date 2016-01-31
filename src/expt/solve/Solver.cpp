/* Solver.cpp
 *
 * Kubo Ryosuke
 */

#include "expt/solve/Solver.hpp"
#include "common/Def.hpp"
#include "common/file_system/Directory.hpp"
#include "common/file_system/FileUtil.hpp"
#include "common/string/StringUtil.hpp"
#include "core/record/CsaReader.hpp"
#include "search/tree/ErrorCounter.hpp"
#include "logger/Logger.hpp"

#include <fstream>

namespace sunfish {

Solver::Solver() {
  searcher_.setHandler(&searchHandler_);
  config_.muximumDepth = 18;
  config_.muximumTimeSeconds = 3;
  result_.corrected = 0;
  result_.incorrected = 0;
  result_.skipped = 0;
}

bool Solver::solve(const char* path) {
  result_.depthSum = 0;
  result_.nodesSum = 0;
  result_.elapsedSum = 0.0;

  if (FileUtil::isDirectory(path)) {
    // 'path' point to a directory
    Directory directory(path);
    auto files = directory.files("*.csa");
    int count = 0;
    for (const auto& path : files) {
      OUT(info) << "------------------------ [" << ++count << "] ------------------------";
      if (!solveCsaFile(path.c_str())) {
        return false;
      }
    }

  } else if (FileUtil::isFile(path)) {
    // 'path' point to a file
    if (!solveCsaFile(path)) {
      return false;
    }

  } else {
    // a specified path is not available.
    LOG(error) << "not exists: " << path;
    return false;
  }

  OUT(info) << "--------------------- completed ---------------------";

  auto percentage = [](float n, float d) {
    return n / d * 100.0f;
  };
  auto total = result_.corrected + result_.incorrected + result_.skipped;
  OUT(info) << "summary:";
  OUT(info) << "  total    : " << total;
  OUT(info) << "  correct  : " << result_.corrected
                               << " (" << percentage(result_.corrected, total) << "%)";
  OUT(info) << "  incorrect: " << result_.incorrected
                               << " (" << percentage(result_.incorrected, total) << "%)";
  OUT(info) << "  skipped  : " << result_.skipped
                               << " (" << percentage(result_.skipped, total) << "%)";
  OUT(info) << "  nps      : " << static_cast<uint64_t>(result_.nodesSum / result_.elapsedSum);
  OUT(info) << "  depth    : " << (static_cast<float>(result_.depthSum) / Searcher::Depth1Ply / (result_.corrected + result_.incorrected));

#if ENABLE_ERR_COUNT
  printErrorCount();
#endif

  return true;
}

bool Solver::solveCsaFile(const char* path) {
  OUT(info) << "[" << path << "]";
  OUT(info) << "";

  std::ifstream file(path);
  if (!file) {
    LOG(error) << "could not open a file: " << path;
    OUT(info) << "";
    return false;
  }

  Record record;
  CsaReader::read(file, record);

  file.close();

  Position position = record.initialPosition;

  for (const auto& move : record.moveList) {
    solve(position, move);

    Piece captured;
    if (!position.doMove(move, captured)) {
      LOG(error) << "an illegal move is detected: " << move.toString(position) << "\n"
                 << position.toString();
      OUT(info) << "";
      return false;
    }
  }

  return true;
}

bool Solver::solve(const Position& position, Move correct) {
  OUT(info) << StringUtil::chomp(position.toString());

  auto config = searcher_.getConfig();
  config.maximumMilliSeconds = config_.muximumTimeSeconds * 1000;
  config.optimumMilliSeconds = config_.muximumTimeSeconds * 1000;
  searcher_.setConfig(config);

  int depth = config_.muximumDepth * Searcher::Depth1Ply;
  searcher_.idsearch(position, depth);

  auto& result = searcher_.getResult();
  auto& info = searcher_.getInfo();
  bool isCorrect = result.move == correct;

  if (result.move.isEmpty() ||
      result.score >= Score::mate() ||
      result.score <= -Score::mate()) {
    OUT(info) << "skipped.";
    result_.skipped++;
    return true;
  }

  if (isCorrect) {
    result_.corrected++;
  } else {
    result_.incorrected++;
  }
  result_.depthSum += result.depth;
  result_.nodesSum += info.nodes + info.quiesNodes;
  result_.elapsedSum += result.elapsed;

  printSearchInfo(OUT(info), info, result.elapsed);
  OUT(info) << "";
  OUT(info) << "answer : " << result.move.toString(position);
  OUT(info) << "correct: " << correct.toString(position);
  OUT(info) << "result : " << (isCorrect ? "correct" : "incorrect");
  OUT(info) << "";

  return true;
}

} // namespace sunfish
