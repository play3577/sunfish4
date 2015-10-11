/* SquareTest.cpp
 *
 * Kubo Ryosuke
 */

#if !defined(NDEBUG)

#include "test/Test.hpp"
#include "core/base/Square.hpp"

using namespace sunfish;

TEST(SquareTest, test) {
  {
    Square sq76(7, 6);
    ASSERT_EQ(S76, sq76);
  }

  {
    Square sq55(5, 5);
    ASSERT_EQ(S54, sq55.up());
    ASSERT_EQ(S56, sq55.down());
    ASSERT_EQ(S65, sq55.left());
    ASSERT_EQ(S45, sq55.right());
    ASSERT_EQ(S53, sq55.up(2));
    ASSERT_EQ(S57, sq55.down(2));
    ASSERT_EQ(S75, sq55.left(2));
    ASSERT_EQ(S35, sq55.right(2));
  }

  {
    Square sq34(3, 4);
    ASSERT_EQ(S76, sq34.reverse());
    ASSERT_EQ(S74, sq34.sym());
  }

  {
    Square sq55(5, 5);
    ASSERT_EQ(S56, sq55.next());

    Square sq59(5, 9);
    ASSERT_EQ(S41, sq59.next());

    Square sq19(1, 9);
    ASSERT_EQ(Square::End, sq19.next());
  }

  {
    Square sq55(5, 5);
    ASSERT_EQ(S45, sq55.nextRightDown());

    Square sq15(1, 5);
    ASSERT_EQ(S96, sq15.nextRightDown());

    Square sq19(1, 9);
    ASSERT_EQ(Square::End, sq19.nextRightDown());
  }
}

#endif // !defined(NDEBUG)
