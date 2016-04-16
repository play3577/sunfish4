/* Solver.hpp
 *
 * Kubo Ryosuke
 */

#ifndef SUNFISH_EXPT_SOLVE_SOLVER_HPP__
#define SUNFISH_EXPT_SOLVE_SOLVER_HPP__

#include "core/position/Position.hpp"
#include "core/move/Move.hpp"
#include "search/Searcher.hpp"
#include <string>
#include <cstdint>

namespace sunfish {

class Solver {
public:

  struct Config {
    int muximumDepth;
    SearchConfig::TimeType muximumTimeSeconds;
  };

  struct Result {
    unsigned corrected;
    unsigned incorrected;

    unsigned mate;

    unsigned depthSum;
    uint64_t nodesSum;
    double elapsedSum;
  };

  Solver();

  bool solve(const char* path);

  bool solve(const std::string& path) {
    return solve(path.c_str());
  }

  const Config& getConfig() const {
    return config_;
  }

  void setConfig(const Config& config) {
    config_ = config;
  }

private:

  bool solveCsaFile(const char* path);

  bool solve(const Position& position, Move correct);

private:

  LoggingSearchHandler searchHandler_;
  Searcher searcher_;
  Config config_;
  Result result_;

};

} // namespace sunfish

#endif // SUNFISH_EXPT_SOLVE_SOLVER_HPP__
