/* Evaluator.hpp
 *
 * Kubo Ryosuke
 */

#ifndef SUNFISH_SEARCH_EVAL_EVALUATOR_HPP__
#define SUNFISH_SEARCH_EVAL_EVALUATOR_HPP__

#include "common/Def.hpp"
#include "core/base/Piece.hpp"
#include "core/move/Move.hpp"
#include "core/position/Position.hpp"
#include "search/eval/FeatureVector.hpp"
#include "search/eval/EvalCache.hpp"
#include "search/eval/Score.hpp"
#include <memory>
#include <cstdint>

namespace sunfish {

inline int32_t positionalScoreScale() {
  return 32;
}

class Evaluator {
public:

  using FVType = FeatureVector<int16_t>;
  using OFVType = OptimizedFeatureVector<int16_t>;

  enum class InitType {
    EvalBin,
    Zero,
  };

  enum class DataSourceType {
    EvalBin,
    Zero,
    Custom,
  };

  static std::shared_ptr<Evaluator> sharedEvaluator();

  Evaluator(InitType type);

  void initializeZero();

  void onChanged(DataSourceType dataSourceType);

  Score calculateMaterialScore(const Position& position) const;

  Score calculateMaterialScoreDiff(Score score,
                                   const Position& position,
                                   Move move,
                                   Piece captured) const;

  Score calculateTotalScore(Score materialScore,
                            const Position& position);

  Score estimateScore(Score score,
                      const Position& position,
                      Move move);

  OFVType& ofv() {
    return ofv_;
  }

  DataSourceType dataSourceType() const {
    return dataSourceType_;
  }

private:

  int32_t calculatePositionalScore(const Position& position);

private:

  EvalCache cache_;

  OFVType ofv_;

  DataSourceType dataSourceType_;

};

bool load(const char* path, Evaluator::FVType& fv);

bool load(Evaluator::FVType& fv);

bool load(const char* path, Evaluator& eval);

bool load(Evaluator& eval);

bool save(const char* path, const Evaluator::FVType& fv);

bool save(const Evaluator::FVType& fv);

} // namespace sunfish

#endif // SUNFISH_SEARCH_EVAL_EVALUATOR_HPP__
