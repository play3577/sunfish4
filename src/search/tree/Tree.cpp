/* Tree.cpp
 *
 * Kubo Ryosuke
 */

#include "search/tree/Tree.hpp"
#include <sstream>

namespace sunfish {

void initializeTree(Tree& tree,
                    const Position& position,
                    Worker* worker) {
  tree.position = position;
  tree.ply = 0;
  tree.worker = worker;
}

bool doMove(Tree& tree, Move& move) {
  auto& node = tree.nodes[tree.ply];
  if (tree.position.doMove(move, node.captured)) {
    node.move = move;
    tree.ply++;

    return true;
  }

  return false;
}

void undoMove(Tree& tree) {
  tree.ply--;
  auto& node = tree.nodes[tree.ply];
  tree.position.undoMove(node.move, node.captured);
}

void doNullMove(Tree& tree) {
  auto& node = tree.nodes[tree.ply];
  tree.position.doNullMove();
  node.move = Move::empty();
  tree.ply++;
}

void undoNullMove(Tree& tree) {
  tree.ply--;
  tree.position.undoNullMove();
}

std::string getPath(const Tree& tree, int ply) {
  std::ostringstream oss;

  for (int i = 0; i < ply; i++) {
    if (i != 0) {
      oss << ' ';
    }

    oss << tree.nodes[i].move.toString();
  }

  return oss.str();
}

} // namespace sunfish
