/* Tree.hpp
 *
 * Kubo Ryosuke
 */

#ifndef SUNFISH_SEARCH_TREE_TREE_HPP__
#define SUNFISH_SEARCH_TREE_TREE_HPP__

#include "common/Def.hpp"
#include "search/tree/Node.hpp"
#include "core/position/Position.hpp"
#include <string>

namespace sunfish {

struct Worker;

struct Tree {
  static CONSTEXPR_CONST int StackSize = 64;

  Position position;
  int ply;
  Node nodes[StackSize];
  Worker* worker;
};

void initializeTree(Tree& tree,
                    const Position& position,
                    Worker* worker);

bool doMove(Tree& tree, Move& move);

void undoMove(Tree& tree);

void doNullMove(Tree& tree);

void undoNullMove(Tree& tree);

std::string getPath(const Tree& tree, int ply);

} // namespace sunfish

#endif // SUNFISH_SEARCH_TREE_TREE_HPP__
