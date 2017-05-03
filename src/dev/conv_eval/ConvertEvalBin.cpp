/* ConvertEvalBin.cpp
 *
 * Kubo Ryosuke
 */

#include "dev/conv_eval/ConvertEvalBin.hpp"
#include "search/eval/Evaluator.hpp"
#include "logger/Logger.hpp"
#include <fstream>
#include <cstring>

namespace sunfish {

bool ConvertEvalBin ::convert() {
  const char* const srcPath = "eval.bin";
  const char* const dstPath = "eval.bin.new";

  auto fv = std::unique_ptr<Evaluator::FVType>(new Evaluator::FVType);

  load(srcPath, *fv.get());

  std::ofstream file(dstPath, std::ios::out | std::ios::binary);
  if (!file) {
    LOG(warning) << "failed to open: " << dstPath;
    return false;
  }

  const char* ver = "2016.09.21.2";
  uint8_t len = strlen(ver);
  file.write(reinterpret_cast<const char*>(&len), sizeof(len));
  file.write(reinterpret_cast<const char*>(ver), len);

  int pieceTypeIndex[] = {
    EvalPieceTypeIndex::BPawn,
    //EvalPieceTypeIndex::WPawn,
    EvalPieceTypeIndex::BLance,
    //EvalPieceTypeIndex::WLance,
    EvalPieceTypeIndex::BKnight,
    //EvalPieceTypeIndex::WKnight,
    EvalPieceTypeIndex::BSilver,
    //EvalPieceTypeIndex::WSilver,
    EvalPieceTypeIndex::BGold,
    //EvalPieceTypeIndex::WGold,
    EvalPieceTypeIndex::BBishop,
    //EvalPieceTypeIndex::WBishop,
    EvalPieceTypeIndex::BHorse,
    //EvalPieceTypeIndex::WHorse,
    EvalPieceTypeIndex::BRook,
    //EvalPieceTypeIndex::WRook,
    EvalPieceTypeIndex::BDragon,
    //EvalPieceTypeIndex::WDragon,
  };

#define WRITE(F) file.write(reinterpret_cast<const char*>(&fv->F), sizeof(fv->F))
#define WRITE_ONE(F) file.write(reinterpret_cast<const char*>(&fv->F), sizeof(int16_t))
  WRITE(kingHand);

  WRITE(kingPieceR);
  WRITE(kingPieceXR);
  WRITE(kingPieceYR);
  WRITE(kingPiece);

  for (int s = 0; s < Square::N; s++) {
    for (int n = 0; n < Neighbor3x3::NN; n++) {
      for (int pti = 0; pti < sizeof(pieceTypeIndex) / sizeof(pieceTypeIndex[0]); pti++) {
        for (int hi = 0; hi < EvalHandIndex::End; hi++) {
          WRITE_ONE(kingNeighborHand[s][n][pieceTypeIndex[pti]][hi]);
        }
      }
    }
  }

  for (int n = 0; n < Neighbor3x3::NN; n++) {
    for (int pti = 0; pti < sizeof(pieceTypeIndex) / sizeof(pieceTypeIndex[0]); pti++) {
      for (int r = 0; r < RelativeSquare::N; r++) {
        for (int pi = 0; pi < EvalPieceIndex::End; pi++) {
          WRITE_ONE(kingNeighborPieceR[n][pieceTypeIndex[pti]][r][pi]);
        }
      }
    }
  }

  for (int s = 0; s < SQUARE_FILES; s++) {
    for (int n = 0; n < Neighbor3x3::NN; n++) {
      for (int pti = 0; pti < sizeof(pieceTypeIndex) / sizeof(pieceTypeIndex[0]); pti++) {
        for (int r = 0; r < RelativeSquare::N; r++) {
          for (int pi = 0; pi < EvalPieceIndex::End; pi++) {
            WRITE_ONE(kingNeighborPieceXR[s][n][pieceTypeIndex[pti]][r][pi]);
          }
        }
      }
    }
  }

  for (int s = 0; s < SQUARE_RANKS; s++) {
    for (int n = 0; n < Neighbor3x3::NN; n++) {
      for (int pti = 0; pti < sizeof(pieceTypeIndex) / sizeof(pieceTypeIndex[0]); pti++) {
        for (int r = 0; r < RelativeSquare::N; r++) {
          for (int pi = 0; pi < EvalPieceIndex::End; pi++) {
            WRITE_ONE(kingNeighborPieceYR[s][n][pieceTypeIndex[pti]][r][pi]);
          }
        }
      }
    }
  }

  for (int s1 = 0; s1 < Square::N; s1++) {
    for (int n = 0; n < Neighbor3x3::NN; n++) {
      for (int pti = 0; pti < sizeof(pieceTypeIndex) / sizeof(pieceTypeIndex[0]); pti++) {
        for (int s2 = 0; s2 < Square::N; s2++) {
          for (int pi = 0; pi < EvalPieceIndex::End; pi++) {
            WRITE_ONE(kingNeighborPiece[s1][n][pieceTypeIndex[pti]][s2][pi]);
          }
        }
      }
    }
  }

  WRITE(kingKingHand);

  for (int s1 = 0; s1 < Square::N; s1++) {
    for (int s2 = 0; s2 < Square::N; s2++) {
      for (int s3 = 0; s3 < Square::N; s3++) {
        for (int pti = 0; pti < sizeof(pieceTypeIndex) / sizeof(pieceTypeIndex[0]); pti++) {
          WRITE_ONE(kingKingPiece[s1][s2][s3][pieceTypeIndex[pti]]);
        }
      }
    }
  }

  WRITE(bRookVer);
  WRITE(kingBRookVerR);
  WRITE(kingBRookVerXR);
  WRITE(kingBRookVerYR);
  WRITE(kingBRookVer);

  WRITE(wRookVer);
  WRITE(kingWRookVerR);
  WRITE(kingWRookVerXR);
  WRITE(kingWRookVerYR);
  WRITE(kingWRookVer);

  WRITE(bRookHor);
  WRITE(kingBRookHorR );
  WRITE(kingBRookHorXR);
  WRITE(kingBRookHorYR);
  WRITE(kingBRookHor);

  WRITE(wRookHor);
  WRITE(kingWRookHorR );
  WRITE(kingWRookHorXR);
  WRITE(kingWRookHorYR);
  WRITE(kingWRookHor);

  WRITE(bBishopDiagL45);
  WRITE(kingBBishopDiagL45R );
  WRITE(kingBBishopDiagL45XR);
  WRITE(kingBBishopDiagL45YR);
  WRITE(kingBBishopDiagL45);

  WRITE(wBishopDiagL45);
  WRITE(kingWBishopDiagL45R );
  WRITE(kingWBishopDiagL45XR);
  WRITE(kingWBishopDiagL45YR);
  WRITE(kingWBishopDiagL45);

  WRITE(bBishopDiagR45);
  WRITE(kingBBishopDiagR45R );
  WRITE(kingBBishopDiagR45XR);
  WRITE(kingBBishopDiagR45YR);
  WRITE(kingBBishopDiagR45);

  WRITE(wBishopDiagR45);
  WRITE(kingWBishopDiagR45R );
  WRITE(kingWBishopDiagR45XR);
  WRITE(kingWBishopDiagR45YR);
  WRITE(kingWBishopDiagR45);

  WRITE(bLance);
  WRITE(kingBLanceR );
  WRITE(kingBLanceXR);
  WRITE(kingBLanceYR);
  WRITE(kingBLance);

  WRITE(wLance);
  WRITE(kingWLanceR );
  WRITE(kingWLanceXR);
  WRITE(kingWLanceYR);
  WRITE(kingWLance);

  file.close();

  return true;
}

} // namespace sunfish
