/* Searcher.cpp
 *
 * Kubo Ryosuke
 */

#include "search/Searcher.hpp"
#include "search/see/SEE.hpp"
#include "search/mate/Mate.hpp"
#include "search/eval/Evaluator.hpp"
#include "search/tree/ErrorCounter.hpp"
#include "core/move/MoveGenerator.hpp"
#include "logger/Logger.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace {

using namespace sunfish;

CONSTEXPR_CONST int AspirationSearchMinDepth = 4 * Searcher::Depth1Ply;

// extensions
CONSTEXPR_CONST int ExtensionDepthOfCheck     = Searcher::Depth1Ply * 1;
CONSTEXPR_CONST int ExtensionDepthOfOneReply  = Searcher::Depth1Ply * 1 / 2;
CONSTEXPR_CONST int ExtensionDepthOfRecapture = Searcher::Depth1Ply * 1 / 4;

/**
 * Check whether the recursive-iterative deepening should be run.
 */
inline bool shouldRecursiveIDSearch(int depth) {
  return depth >= 3 * Searcher::Depth1Ply;
}

/**
 * Calculate a depth of recursive iterative deepening.
 */
inline int recursiveIDSearchDepth(int depth) {
  return depth < Searcher::Depth1Ply * 9 / 2 ?  Searcher::Depth1Ply * 3 / 2 :
                                                depth - Searcher::Depth1Ply * 3;
}

/**
 * Calculate a depth of null move search.
 */
inline int nullDepth(int depth) {
  return depth <  Searcher::Depth1Ply * 26 / 4 ? depth - Searcher::Depth1Ply * 12 / 4 :
        (depth <= Searcher::Depth1Ply * 30 / 4 ? Searcher::Depth1Ply * 14 / 4 :
                                                 depth - Searcher::Depth1Ply * 16 / 4); 
}

/**
 * values for reducing from the depth.
 */
uint8_t ReductionDepth[32][2][2];

void initializeReductionDepth() {
  for (int hist = 0; hist < 32; hist++) {
    float r = std::pow(1.0f - hist/32.0f, 2.0f) * Searcher::Depth1Ply;
    ReductionDepth[hist][0][0] = r * 1.4f;
    ReductionDepth[hist][0][1] = r * 2.0f;
    ReductionDepth[hist][1][0] = r * 2.8f;
    ReductionDepth[hist][1][1] = r * 3.8f;
  }
}

/**
 * Returns a value for reducing from the depth.
 */
int reductionDepth(int depth,
                   History::CountType hist,
                   bool isNullWindow,
                   bool improving) {
  static_assert(History::Scale >> 8 == 32, "invalid range");
  return ReductionDepth[hist >> 8]
                       [(!improving && depth < 9 * Searcher::Depth1Ply) ? 1 : 0]
                       [isNullWindow ? 1: 0];
}

/** the maximum depth to perform futility pruning. */
CONSTEXPR_CONST int FutilityPruningMaxDepth = 9 * Searcher::Depth1Ply;

Score FutilityPruningMargin[9][32];

void initializeFutilityPruningMargin() {
  for (int depth = 0; depth < 9; depth++) {
    for (int count = 0; count < 32; count++) {
      Score margin = 320 * std::log(2.0f * (depth + 1.0f)) / std::log(4.0f) - 32 * count;
      FutilityPruningMargin[depth][count] = std::max(margin, Score(200));
    }
  }
}

/**
 * Returns the margin for futility pruning.
 */
Score futilityPruningMargin(int depth,
                            int count) {
  return FutilityPruningMargin[std::max(depth / Searcher::Depth1Ply, 0)]
                              [std::min(count / 4, 31)];
}

#if 0
void printMoves(const Position& pos,
                const Moves& moves) {
  std::ostringstream oss;
  for (const auto& move : moves) {
    Score score = moveToScore(move);
    oss << move.toString(pos) << '(' << score << ") ";
  }
  OUT(info) << oss.str();
}
#endif

} // namespace

namespace sunfish {

void Searcher::initialize() {
  initializeReductionDepth();
  initializeFutilityPruningMargin();
}

Searcher::Searcher() :
  config_ (getDefaultSearchConfig()),
  evaluator_(Evaluator::sharedEvaluator()),
  handler_(nullptr) {
}

Searcher::Searcher(std::shared_ptr<Evaluator> evaluator) :
  config_ (getDefaultSearchConfig()),
  evaluator_(evaluator),
  handler_(nullptr) {
}

void Searcher::onSearchStarted() {
  timer_.start();

  interrupted_ = false;

  result_.move = Move::empty();
  result_.score = -Score::infinity();
  result_.pv.clear();
  result_.depth = 0;
  result_.elapsed = 0.0f;

  initializeWorker(workerOnMainThread_);

  history_.reduce();

  gain_.clear();

  if (handler_ != nullptr) {
    handler_->onStart(*this);
  }
}

void Searcher::updateInfo() {
  initializeSearchInfo(info_);
  mergeSearchInfo(info_, workerOnMainThread_.info);
}

/**
 * search of root node
 */
void Searcher::search(const Position& pos,
                      int depth,
                      Score alpha,
                      Score beta,
                      Record* record /*= nullptr*/) {
  onSearchStarted();

  auto& tree = treeOnMainThread_;
  auto& worker = workerOnMainThread_;
  initializeTree(tree,
                 pos,
                 *evaluator_,
                 &worker,
                 record);

  auto& node = tree.nodes[tree.ply];
  arrive(node);

  node.checkState = tree.position.getCheckState();

  generateMoves(tree);

  Move bestMove = Move::empty();

  bool isFirst = true;

  // expand the branches
  for (int moveCount = 0; ; moveCount++) {
    Move move = nextMove(tree);
    if (move.isEmpty()) {
      break;
    }

    int newDepth = depth - Depth1Ply;

    // late move reduction
    int reduced = 0;
    if (!isFirst &&
        !isCheck(node.checkState) &&
        newDepth >= Depth1Ply &&
        !isTacticalMove(tree.position, move)) {
      auto turn = tree.position.getTurn();
      reduced = reductionDepth(newDepth,
                               history_.ratio(turn, move),
                               false,
                               true);
      newDepth = newDepth - reduced;
    }

    bool moveOk = doMove(tree, move, *evaluator_);
    if (!moveOk) {
      moveCount--;
      continue;
    }

    NodeStat newNodeStat = NodeStat::normal();

    Score score;
    if (isFirst) {
      score = -search(tree,
                      newDepth,
                      -beta,
                      -alpha,
                      newNodeStat);
    } else {
      // nega-scout
      score = -search(tree,
                      newDepth,
                      -(alpha + 1),
                      -alpha,
                      newNodeStat);

#if ENABLE_ERR_COUNT
      if (score < alpha) {
        if (-search(tree,
                    newDepth + reduced,
                    -(alpha + 1),
                    -alpha,
                    newNodeStat) <= alpha) {
          EC_SUCCESS(LMR, newDepth);
        } else {
          EC_ERROR(LMR, newDepth);
        }
      }
#endif // ENABLE_ERR_COUNT

      if (!isInterrupted() && score > alpha && reduced != 0) {
        newDepth = newDepth + reduced;
        score = -search(tree,
                        newDepth,
                        -(alpha + 1),
                        -alpha,
                        newNodeStat);
      }

      if (!isInterrupted() && score > alpha && score < beta) {
        score = -search(tree,
                        newDepth,
                        -beta,
                        -alpha,
                        newNodeStat);
      }
    }

    undoMove(tree);

    updateInfo();

    if (isInterrupted()) {
      break;
    }

    if (score > alpha) {
      alpha = score;
      bestMove = move;
      auto& childNode = tree.nodes[tree.ply+1];
      node.pv.set(move, depth, childNode.pv);

      if (score >= beta) {
        break;
      }
    }

    isFirst = false;
  }

  result_.move = bestMove;
  result_.score = alpha;
  result_.pv = node.pv;
  result_.depth = depth;
  result_.elapsed = timer_.elapsed();
}

/**
 * iterative deepening search
 */
void Searcher::idsearch(const Position& pos,
                        int depth,
                        Record* record /*= nullptr*/) {
  onSearchStarted();

  auto& tree = treeOnMainThread_;
  auto& worker = workerOnMainThread_;
  initializeTree(tree,
                 pos,
                 *evaluator_,
                 &worker,
                 record);

  auto& node = tree.nodes[tree.ply];
  arrive(node);

  node.checkState = tree.position.getCheckState();

  // generate moves
  node.moves.clear();
  if (!isCheck(node.checkState)) {
    MoveGenerator::generateCapturingMoves(tree.position, node.moves);
    MoveGenerator::generateNotCapturingMoves(tree.position, node.moves);
  } else {
    MoveGenerator::generateEvasions(tree.position, node.checkState, node.moves);
  }

  sortRootMoves(tree);

  if (node.moves.size() == 0) {
    return;
  }

  if (isInterrupted()) {
    return;
  }

  int completedDepth = 0;
  for (int currDepth = Depth1Ply;; currDepth += Depth1Ply) {
    bool cont = aspsearch(tree, currDepth);

    if (isInterrupted()) {
      break;
    }

    completedDepth = currDepth;

    if (!cont || currDepth >= depth) {
      break;
    }
  }

  result_.move = node.moves[0].excludeExtData();
  result_.score = moveToScore(node.moves[0]);
  result_.pv = node.pv;
  result_.depth = completedDepth;
  result_.elapsed = timer_.elapsed();
}

/**
 * aspiration search
 */
bool Searcher::aspsearch(Tree& tree,
                         int depth) {
  auto& node = tree.nodes[tree.ply];

  if (node.moves.size() == 0) {
    return false;
  }

  bool doAsp = depth >= AspirationSearchMinDepth;

  Score prevScore = moveToScore(node.moves[0]);
  Score alphas[] = {
    prevScore - 128,
    prevScore - 256,
    -Score::infinity()
  };
  Score betas[] = {
    prevScore + 128,
    prevScore + 256,
    Score::infinity()
  };
  int alphaIndex = doAsp ? 0 : 2;
  int betaIndex = doAsp ? 0 : 2;

  Score bestScore = -Score::infinity();

  bool isFirst = true;

  // expand the branches
  for (Moves::size_type moveCount = 0; moveCount < node.moves.size();) {
    Score alpha = std::max(alphas[alphaIndex], bestScore);
    Score beta = betas[betaIndex];

    if (bestScore >= beta) {
      LOG(warning) << "invalid state.";
    }

    Move move = node.moves[moveCount];
    int newDepth = depth - Depth1Ply;

    // late move reduction
    int reduced = 0;
    if (!isFirst &&
        !isCheck(node.checkState) &&
        newDepth >= Depth1Ply &&
        !isTacticalMove(tree.position, move)) {
      auto turn = tree.position.getTurn();
      reduced = reductionDepth(newDepth,
                               history_.ratio(turn, move),
                               false,
                               true);
      newDepth = newDepth - reduced;
    }

    bool moveOk = doMove(tree, move, *evaluator_);
    if (!moveOk) {
      LOG(warning) << "invalid state.";
      node.moves.remove(moveCount);
      continue;
    }

    NodeStat newNodeStat = NodeStat::normal();

    Score score;
    if (isFirst) {
      score = -search(tree,
                      newDepth,
                      -beta,
                      -alpha,
                      newNodeStat);
    } else {
      // nega-scout
      score = -search(tree,
                      newDepth,
                      -(alpha + 1),
                      -alpha,
                      newNodeStat);

#if ENABLE_ERR_COUNT
      if (score < alpha) {
        if (-search(tree,
                    newDepth + reduced,
                    -(alpha + 1),
                    -alpha,
                    newNodeStat) <= alpha) {
          EC_SUCCESS(LMR, newDepth);
        } else {
          EC_ERROR(LMR, newDepth);
        }
      }
#endif // ENABLE_ERR_COUNT

      if (!isInterrupted() && score > alpha && reduced != 0) {
        newDepth = newDepth + reduced;
        score = -search(tree,
                        newDepth,
                        -(alpha + 1),
                        -alpha,
                        newNodeStat);
      }

      if (!isInterrupted() && score > alpha && score < beta) {
        score = -search(tree,
                        newDepth,
                        -beta,
                        -alpha,
                        newNodeStat);
      }
    }

    undoMove(tree);

    updateInfo();

    if (isInterrupted()) {
      break;
    }

    // fail-low
    if (score <= alphas[alphaIndex] && score >= bestScore) {
      alphaIndex++;

      auto& childNode = tree.nodes[tree.ply+1];
      node.pv.set(move, depth, childNode.pv);

      if (handler_ != nullptr) {
        handler_->onFailLow(*this, node.pv, timer_.elapsed(), depth, score);
      }
      continue;
    }

    // fail-high
    if (score >= beta && beta != Score::infinity()) {
      betaIndex++;

      auto& childNode = tree.nodes[tree.ply+1];
      node.pv.set(move, depth, childNode.pv);

      if (handler_ != nullptr) {
        handler_->onFailHigh(*this, node.pv, timer_.elapsed(), depth, score);
      }
      continue;
    }

    if (score > bestScore) {
      bestScore = score;

      auto& childNode = tree.nodes[tree.ply+1];
      node.pv.set(move, depth, childNode.pv);
    }

    // insertion
    int i;
    for (i = moveCount - 1; i >= 0; i--) {
      if (moveToScore(node.moves[i]) >= score) { break; }
      node.moves[i+1] = node.moves[i];
    }
    node.moves[i+1] = move;
    setScoreToMove(node.moves[i+1], score);

    moveCount++;

    isFirst = false;
  }

  if (node.pv.size() != 0 && bestScore != -Score::infinity()) {
    storePV(tree,
            node.pv,
            0,
            bestScore);

    if (handler_ != nullptr) {
      handler_->onUpdatePV(*this, node.pv, timer_.elapsed(), depth, bestScore);
    }
  }

  return bestScore > -Score::mate() && bestScore < Score::mate();
}

/**
 * search of internal nodes
 */
Score Searcher::search(Tree& tree,
                       int depth,
                       Score alpha,
                       Score beta,
                       NodeStat nodeStat) {
#if 0
  bool isDebug = false;
  if (getPath(tree, tree.ply) == "4647+ 5847 46FU") {
    LOG(info) << "debugging node:"
              << " depth=" << depth
              << " alpha=" << alpha
              << " beta =" << beta;
    isDebug = true;
  }
#endif

  auto& node = tree.nodes[tree.ply];
  arrive(node);

  // SHEK(strong horizontal effect killer)
  switch (tree.shekTable.check(tree.position)) {
  case ShekState::Equal4:
    node.isHistorical = true;
    switch (tree.scr.detect(tree)) {
    case SCRState::Draw:
      return Score::zero();
    case SCRState::Win:
      return Score::infinity() - tree.ply;
    case SCRState::Lose:
      return -Score::infinity() + tree.ply;
    case SCRState::None:
      break;
    }

  case ShekState::Equal:
    node.isHistorical = true;
    return Score::zero();

  case ShekState::Superior:
    node.isHistorical = true;
    return Score::infinity() - tree.ply;

  case ShekState::Inferior:
    node.isHistorical = true;
    return -Score::infinity() + tree.ply;

  case ShekState::None:
    break;
  }

  // quiesence search
  if (depth <= 0) {
    return quies(tree,
                 0,
                 alpha,
                 beta);
  }

  auto& worker = *tree.worker;
  worker.info.nodes++;

  if (tree.ply == Tree::StackSize - 2) {
    node.isHistorical = true;
    return calculateStandPat(tree);
  }

  const Score oldAlpha = alpha;

  // distance pruning
  {
    Score lowerScore = -Score::infinity() + tree.ply;
    Score upperScore = Score::infinity() - tree.ply - 1;
    if (lowerScore >= beta) {
      return lowerScore;
    } else if (lowerScore > alpha) {
      alpha = lowerScore;
    } else if (upperScore <= alpha) {
      return upperScore;
    }
  }

  bool isNullWindow = oldAlpha + 1 == beta;

  node.checkState = tree.position.getCheckState();

  // transposition table
  TTElement tte;
  if (tt_.get(tree.position.getHash(), tte)) {
    auto ttScoreType = tte.scoreType();
    Score ttScore = tte.score(tree.ply);
    int ttDepth = tte.depth();

    bool isMate = (ttScore <= -Score::mate() && (ttScoreType == TTScoreType::Exact ||
                                                 ttScoreType == TTScoreType::Upper)) ||
                  (ttScore >=  Score::mate() && (ttScoreType == TTScoreType::Exact ||
                                                 ttScoreType == TTScoreType::Lower));

    // cut
    if (nodeStat.isHashCut() &&
        isNullWindow &&
        (ttDepth >= depth || isMate)) {
      if (ttScoreType == TTScoreType::Exact ||
         (ttScoreType == TTScoreType::Upper && ttScore <= oldAlpha) ||
         (ttScoreType == TTScoreType::Lower && ttScore >= beta)) {
        worker.info.hashCut++;
        return ttScore;
      }
    }

    // if the score is larger than beta by a considerable margin.
    if (nodeStat.isHashCut() &&
        isNullWindow &&
        (ttScoreType == TTScoreType::Exact ||
         ttScoreType == TTScoreType::Lower) &&
        !isCheck(node.checkState) &&
        !isCheck(tree.nodes[tree.ply-1].checkState) &&
        depth < FutilityPruningMaxDepth &&
        ttScore >= beta + futilityPruningMargin(depth, 0)) {
#if ENABLE_ERR_COUNT
      if (search(tree, depth, alpha, beta, nodeStat.unsetHashCut()) >= beta) {
        EC_SUCCESS(futilityPruning, depth);
      } else {
        EC_ERROR(futilityPruning, depth);
      }
#endif // ENABLE_ERR_COUNT
      return beta;
    }

    if (!shouldRecursiveIDSearch(depth) ||
        ttDepth >= recursiveIDSearchDepth(depth)) {
      if (ttScoreType == TTScoreType::Exact ||
          ttScoreType == TTScoreType::Upper) {
        // if the score is smaller than alpha, exclude recursive iterative deepening search.
        if (ttScore < alpha && ttDepth >= recursiveIDSearchDepth(depth)) {
          nodeStat.unsetRecursiveIDSearch();
        }
        // if the score is smaller than beta, exclude null window search.
        if (ttScore < beta && ttDepth >= nullDepth(depth)) {
          nodeStat.unsetNullMoveSearch();
        }
      }

      // previous best move
      Move ttMove = tte.move();
      if (tree.position.isLegalMoveMaybe(ttMove, node.checkState)) {
        node.hashMove = ttMove;
      }
    }

    if (tte.isMateThreat()) {
      nodeStat.setMateThreat();
    }
  }

  if (nodeStat.isMateDetection() &&
      !isCheck(node.checkState) &&
      Mate::mate1Ply(tree.position)) {
    return Score::infinity() - tree.ply - 1;
  }

  Score standPat = calculateStandPat(tree);

  // null move pruning
  if (isNullWindow &&
      nodeStat.isNullMoveSearch() &&
      !nodeStat.isMateThreat() &&
      !isCheck(node.checkState) &&
      standPat >= beta &&
      depth >= Depth1Ply * 2) {
    int newDepth = nullDepth(depth);
    NodeStat newNodeStat = NodeStat::normal().unsetNullMoveSearch();

    doNullMove(tree);

    Score score = -search(tree,
                          newDepth,
                          -beta,
                          -alpha,
                          newNodeStat);

    undoNullMove(tree);

    if (score >= beta) {
      auto& childNode = tree.nodes[tree.ply+1];
      node.isHistorical = childNode.isHistorical;
      worker.info.nullMovePruning++;
      tt_.store(tree.position.getHash(),
                oldAlpha,
                beta,
                score,
                depth,
                tree.ply,
                Move::empty(),
                false);
      return score;
    }

    if (score < -Score::mate()) {
      nodeStat.setMateThreat();
    }
  }

  // recursive iterative deepening
  if (node.hashMove.isEmpty() &&
      nodeStat.isRecursiveIDSearch() &&
      shouldRecursiveIDSearch(depth)) {
    int newDepth = recursiveIDSearchDepth(depth);
    NodeStat newNodeStat = NodeStat::normal().unsetNullMoveSearch()
                                             .unsetHashCut()
                                             .unsetMateDetection();

    search(tree,
           newDepth,
           alpha,
           beta,
           newNodeStat);

    if (isInterrupted()) {
      return Score::zero();
    }

    TTElement tte;
    if (tt_.get(tree.position.getHash(), tte)) {
      Move ttMove = tte.move();
      if (tree.position.isLegalMoveMaybe(ttMove, node.checkState)) {
        node.hashMove = ttMove;
      }
    }

    arrive(node);
  }

  bool isFirst = true;
  bool improving = isImproving(tree);
  Move bestMove = Move::empty();

  generateMoves(tree);

  // expand branches
  for (int moveCount = 0; ; moveCount++) {
    Move move = nextMove(tree);
    if (move.isEmpty()) {
      break;
    }

    bool currentMoveIsCheck = tree.position.isCheck(move);
    int newDepth = depth - Depth1Ply;
    NodeStat newNodeStat = NodeStat::normal();

    // extensions
    if (currentMoveIsCheck) {
      newDepth += ExtensionDepthOfCheck;

    } else if (isFirst &&
               isCheck(node.checkState) &&
               node.moveIterator == node.moves.end()) {
      newDepth += ExtensionDepthOfOneReply;

    } else if (!isCheck(node.checkState) &&
               nodeStat.isRecaptureExtension() &&
               isRecapture(tree, move)) {
      newDepth += ExtensionDepthOfRecapture;
      nodeStat.unsetRecaptureExtension();
      newNodeStat.unsetRecaptureExtension();
    }

    // late move reduction
    int reduced = 0;
    if (!isFirst &&
        newDepth >= Depth1Ply &&
        !nodeStat.isMateThreat() &&
        !isCheck(node.checkState) &&
        !isPriorMove(tree, move) &&
        !isTacticalMove(tree.position, move)) {
      auto turn = tree.position.getTurn();
      reduced = reductionDepth(newDepth,
                               history_.ratio(turn, move),
                               isNullWindow,
                               improving);
      newDepth = newDepth - reduced;
    }

    // futility pruning
    bool doFutilityPruning = !currentMoveIsCheck &&
                             !isCheck(node.checkState) &&
                             newDepth < FutilityPruningMaxDepth &&
                             alpha > -Score::mate();
    Score estScore;
    if (doFutilityPruning) {
      Score futAlpha = alpha - futilityPruningMargin(newDepth, moveCount);
      estScore = estimateScore(tree, move, *evaluator_);
      if (estScore + gain_.get(move, targetPiece(tree, move)) <= futAlpha) {
        isFirst = false;
        worker.info.futilityPruning++;
#if ENABLE_ERR_COUNT
        if (doMove(tree, move, *evaluator_)) {
          if (-search(tree, newDepth, -beta, -alpha, newNodeStat) <= alpha) {
            EC_SUCCESS(futilityPruning, newDepth);
          } else {
            EC_ERROR(futilityPruning, newDepth);
          }
          undoMove(tree);
        }
#endif // ENABLE_ERR_COUNT
        continue;
      }
    }

    // prune negative SEE moves
    if (!currentMoveIsCheck &&
        !isCheck(node.checkState) &&
        newDepth < Depth1Ply * 2 &&
        !isPriorMove(tree, move) &&
        !isTacticalMove(tree.position, move) &&
        SEE::calculate(tree.position, move) < Score::zero()) {
      isFirst = false;
      continue;
    }

    bool moveOk = doMove(tree, move, *evaluator_);
    if (!moveOk) {
      moveCount--;
      continue;
    }

    Score newStandPat = -calculateStandPat(tree);

    Score score;
    if (isFirst) {
      score = -search(tree,
                      newDepth,
                      -beta,
                      -alpha,
                      newNodeStat);
    } else {
      // nega-scout
      score = -search(tree,
                      newDepth,
                      -(alpha + 1),
                      -alpha,
                      newNodeStat);

#if ENABLE_ERR_COUNT
      if (score < alpha) {
        if (-search(tree,
                    newDepth + reduced,
                    -(alpha + 1),
                    -alpha,
                    newNodeStat) <= alpha) {
          EC_SUCCESS(LMR, newDepth);
        } else {
          EC_ERROR(LMR, newDepth);
        }
      }
#endif // ENABLE_ERR_COUNT

      if (!isInterrupted() &&
          score > alpha &&
          reduced != 0) {
        newDepth = newDepth + reduced;
        score = -search(tree,
                        newDepth,
                        -(alpha + 1),
                        -alpha,
                        newNodeStat);
      }

      if (!isInterrupted() &&
          score > alpha &&
          score < beta &&
          !isNullWindow) {
        score = -search(tree,
                        newDepth,
                        -beta,
                        -alpha,
                        newNodeStat);
      }
    }

    undoMove(tree);

    if (isInterrupted()) {
      return Score::zero();
    }

    if (doFutilityPruning) {
      Score exactScore = score <= alpha ? newStandPat
                       : std::max(score, newStandPat);
      gain_.update(move,
                   targetPiece(tree, move),
                   exactScore - estScore);
    }

    auto& childNode = tree.nodes[tree.ply+1];

    if (score > alpha) {
      alpha = score;
      bestMove = move;

      // beta cut
      if (score >= beta) {
        node.isHistorical = childNode.isHistorical;
        worker.info.failHigh++;
        if (isFirst) {
          worker.info.failHighFirst++;
        }
        break;
      }

      node.pv.set(move, depth, childNode.pv);
    }

    node.isHistorical |= childNode.isHistorical;

    isFirst = false;
  }

  if (!bestMove.isEmpty() &&
      !isCheck(node.checkState)) {
    // killer move
    addKiller(tree, bestMove);

    // history heuristics
    unsigned hval = std::max(depth * 2 / Depth1Ply, 1);
    for (auto& move : node.moves) {
      history_.add(tree.position.getTurn(),
                   move,
                   hval,
                   move == bestMove ? hval : 0);
    }
  }

  if (!node.isHistorical) {
    tt_.store(tree.position.getHash(),
              oldAlpha,
              beta,
              alpha,
              depth,
              tree.ply,
              bestMove,
              nodeStat.isMateThreat());
  }

  return alpha;
}

/**
 * quiesence search
 */
Score Searcher::quies(Tree& tree,
                      int qply,
                      Score alpha,
                      Score beta) {
  auto& node = tree.nodes[tree.ply];
  arrive(node);

  auto& worker = *tree.worker;
  worker.info.quiesNodes++;

  Score standPat = calculateStandPat(tree);

  if (standPat >= beta) {
    return standPat;
  }

  if (tree.ply == Tree::StackSize - 2) {
    node.isHistorical = true;
    return standPat;
  }

  alpha = std::max(alpha, standPat);

  node.checkState = tree.position.getCheckState();

  generateMovesOnQuies(tree,
                       qply,
                       alpha);

  // expand the branches
  for (;;) {
    Move move = nextMoveOnQuies(node);
    if (move.isEmpty()) {
      break;
    }

    bool moveOk = doMove(tree, move, *evaluator_);
    if (!moveOk) {
      continue;
    }

    Score score = -quies(tree,
                         qply + 1,
                         -beta,
                         -alpha);

    undoMove(tree);

    if (score > alpha) {
      alpha = score;

      auto& childNode = tree.nodes[tree.ply+1];
      node.pv.set(move, 0, childNode.pv);

      // beta cut
      if (score >= beta) {
        break;
      }
    }
  }

  return alpha;
}

/**
 * move generation for full expanding nodes
 */
void Searcher::generateMoves(Tree& tree) {
  auto& node = tree.nodes[tree.ply];
  auto& parentNode = tree.nodes[tree.ply-1];

  node.moves.clear();
  node.moveIterator = node.moves.begin();

  if (!node.hashMove.isEmpty()) {
    node.moves.add(node.hashMove);
  }

  if (!isCheck(node.checkState) &&
      hasKiller1(tree) &&
      isKiller1Good(tree) &&
      isKiller1Legal(tree)) {
    node.moves.add(parentNode.killerMove1);
  }

  if (!isCheck(node.checkState) &&
      hasKiller2(tree) &&
      isKiller2Good(tree) &&
      isKiller2Legal(tree)) {
    node.moves.add(parentNode.killerMove2);
  }

  if (!isCheck(node.checkState)) {
    node.genPhase = GenPhase::CapturingMoves;
  } else {
    node.genPhase = GenPhase::Evasions;
  }
}

Move Searcher::nextMove(Tree& tree) {
  auto& node = tree.nodes[tree.ply];

  for (;;) {
    if (node.moveIterator != node.moves.end() &&
        // if the move has minus SEE value, carry foward it to NotCapturingMoves phase.
        (node.genPhase != GenPhase::NotCapturingMoves ||
         moveToScore(*node.moveIterator) >= Score::zero())) {
      return *(node.moveIterator++);
    }

    switch (node.genPhase) {
    case GenPhase::CapturingMoves:
      MoveGenerator::generateCapturingMoves(tree.position, node.moves);
      remove(node.moves, node.moveIterator, [&tree](const Move& move) {
        return isPriorMove(tree, move);
      });
      SEE::sortMoves(tree.position,
                     node.moves,
                     node.moveIterator,
                     false /* excludeSmallCaptures */);
      node.genPhase = GenPhase::NotCapturingMoves;
      break;

    case GenPhase::NotCapturingMoves:
      MoveGenerator::generateNotCapturingMoves(tree.position,
                                               node.moves);
      remove(node.moves, node.moveIterator, [&tree](const Move& move) {
        return isPriorMove(tree, move);
      });
      sortMovesOnHistory(tree);
      node.genPhase = GenPhase::End;
      break;

    case GenPhase::Evasions:
      MoveGenerator::generateEvasions(tree.position, node.checkState, node.moves);
      sortMovesOnHistory(tree);
      node.genPhase = GenPhase::End;
      break;

    case GenPhase::End:
      return Move::empty();

    }
  }
}

/**
 * move generation for nodes of quiesence search
 */
void Searcher::generateMovesOnQuies(Tree& tree,
                                    int qply,
                                    Score alpha) {
  auto& node = tree.nodes[tree.ply];
  auto& worker = *tree.worker;

  node.moves.clear();
  node.moveIterator = node.moves.begin();

  if (!isCheck(node.checkState)) {
    MoveGenerator::generateCapturingMoves(tree.position, node.moves);
    bool excludeSmallCaptures = qply >= 7;

    // futility pruning
    for (auto ite = node.moveIterator; ite != node.moves.end(); ) {
      auto& move = *ite;

      Score estScore = estimateScore(tree, move, *evaluator_)
                     + gain_.get(move, targetPiece(tree, move));
      if (estScore <= alpha) {
        ite = node.moves.remove(ite);
        worker.info.futilityPruning++;
        continue;
      }

      ite++;
    }

    SEE::sortMoves(tree.position,
                   node.moves,
                   node.moveIterator,
                   excludeSmallCaptures);
  } else {
    MoveGenerator::generateEvasions(tree.position, node.checkState, node.moves);
    sortMovesOnHistory(tree);
  }
}

Move Searcher::nextMoveOnQuies(Node& node) {
  if (node.moveIterator == node.moves.end()) {
    return Move::empty();
  }

  return *(node.moveIterator++);
}

void Searcher::sortMovesOnHistory(Tree& tree) {
  auto& node = tree.nodes[tree.ply];
  auto turn = tree.position.getTurn();

  for (auto ite = node.moveIterator; ite != node.moves.end(); ite++) {
    auto& move = *ite;
    auto r = history_.ratio(turn, move);
    move.setExtData(static_cast<Move::RawType16>(r));
  }

  std::sort(node.moveIterator, node.moves.end(), [](const Move& lhs, const Move& rhs) {
    return lhs.extData() > rhs.extData();
  });
}

void Searcher::sortRootMoves(Tree& tree) {
  auto& node = tree.nodes[tree.ply];

  random_.shuffle(node.moves.begin(), node.moves.end());

  Move ttMove = Move::empty();
  TTElement tte;
  if (tt_.get(tree.position.getHash(), tte)) {
    ttMove = tte.move();
  }

  for (Moves::size_type moveCount = 0; moveCount < node.moves.size();) {
    Move move = node.moves[moveCount];

    bool moveOk = doMove(tree, move, *evaluator_);
    if (!moveOk) {
      node.moves.remove(moveCount);
      continue;
    }

    if (move == ttMove) {
      undoMove(tree);
      setScoreToMove(node.moves[moveCount], Score::infinity());
      moveCount++;
      continue;
    }

    Score score = quies(tree,
                        0,
                        -Score::infinity(),
                        Score::infinity());

    if (tt_.get(tree.position.getHash(), tte)) {
      auto ttScoreType = tte.scoreType();
      auto ttScore = tte.score(1);
      if (ttScoreType == TTScoreType::Exact) {
        score = ttScore;
      } else if (ttScoreType == TTScoreType::Lower) {
        score = std::max(score, ttScore);
      } else if (ttScoreType == TTScoreType::Upper) {
        score = std::min(score, ttScore);
      }
    }

    setScoreToMove(node.moves[moveCount], -score);

    undoMove(tree);

    moveCount++;
  }

  std::sort(node.moves.begin(), node.moves.end(), [](Move lhs, Move rhs) {
    return moveToScore(lhs) > moveToScore(rhs);
  });
}

void Searcher::storePV(Tree& tree,
                       const PV& pv,
                       unsigned ply,
                       Score score) {
  if (ply >= pv.size()) {
    return;
  }

  int depth = pv.getDepth(ply);
  if (depth <= 0) {
    return;
  }

  Move move = pv.getMove(ply);
  if (move.isEmpty()) {
    LOG(warning) << "the PV contain an invalid move.";
    return;
  }

  if (doMove(tree, move, *evaluator_)) {
    storePV(tree,
            pv,
            ply + 1,
            -score);
    undoMove(tree);
    tt_.storePV(tree.position.getHash(),
                score,
                depth,
                move);
  } else {
    LOG(warning) << "the PV contain an illegal move.";
  }
}

} // namespace sunfish
