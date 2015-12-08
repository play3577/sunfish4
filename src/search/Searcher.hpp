/* Searcher.hpp
 *
 * Kubo Ryosuke
 */

#ifndef SUNFISH_SEARCH_SEARCHER_HPP__
#define SUNFISH_SEARCH_SEARCHER_HPP__

#include "eval/Value.hpp"
#include "search/tree/Tree.hpp"
#include "search/eval/Evaluator.hpp"
#include "common/math/Random.hpp"
#include <atomic>

namespace sunfish {

class Position;
class Move;

class Searcher {
public:

  static CONSTEXPR_CONST int Depth1Ply = 8;

  struct Info {
    Move move;
    Value value;
    PV pv;
  };

  Searcher();

  bool search(const Position& pos,
              int depth) {
    return search(pos,
                  depth,
                  -Value::infinity(),
                  Value::infinity());
  }

  bool search(const Position& pos,
              int depth,
              Value alpha,
              Value beta);

  const Info& getInfo() {
    return info_;
  }

  void interrupt() {
    interrupted_ = true;
  }

private:

  void onSearchStarted();

  Value search(Tree& tree,
               int depth,
               Value alpha,
               Value beta);

  Value quies(Tree& tree,
              Value alpha,
              Value beta);

  Move nextMove(Node& node);

  Random random_;

  Evaluator evaluator_;
  Tree treeOnMainThread_;

  Info info_;

  std::atomic<bool> interrupted_;

};

} // namespace sunfish

#endif // SUNFISH_SEARCH_SEARCHER_HPP__
