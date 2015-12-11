/* UsiClient.cpp
 *
 * Kubo Ryosuke
 */

#include "usi/client/UsiClient.hpp"
#include "common/util/StringUtil.hpp"
#include "core/record/SfenParser.hpp"
#include "search/eval/Material.hpp"
#include "logger/Logger.hpp"
#include <sstream>
#include <utility>
#include <unordered_map>
#include <functional>
#include <cctype>
#include <cstdint>
#include <cstdlib>

namespace {

const char* ProgramName = "Sunfish4";
const char* Author = "Kubo Ryosuke";

} // namespace

namespace sunfish {

UsiClient::UsiClient() :
    state_(State::None),
    positionIsInitialized_(false) {
  searcher_.setHandler(this);
}

bool UsiClient::start() {
  if (!checkStateIn(State::None)) {
    Loggers::warning << "invalid state: " << toString(state_);
    return false;
  }

  changeState(State::Ready);

  bool usiAccepted = acceptUsiCommand();
  if (!usiAccepted) {
    Loggers::error << "invalid command is received.";
    return false;
  }

  bool ok = runCommandLoop();

  changeState(State::None);

  return ok;
}

bool UsiClient::acceptUsiCommand() {
  auto command = receive();
  if (command != "usi") {
    return false;
  }

  send("id", "name", ProgramName);
  send("id", "author", Author);

  // TODO: send options

  send("usiok");

  return true;
}

bool UsiClient::runCommandLoop() {
#define MAKE_COMMAND_HANDLER(func) ([this](const CommandArguments& args) { return func(args); })
  using Handler = bool(const CommandArguments&);
  using HandlerWrapper = std::function<Handler>;
  std::unordered_map<std::string, HandlerWrapper> handlerMap = {
    { "isready", MAKE_COMMAND_HANDLER(onIsReady) },
    { "setoption", MAKE_COMMAND_HANDLER(onSetOption) },
    { "usinewgame", MAKE_COMMAND_HANDLER(onUsiNewGame) },
    { "position", MAKE_COMMAND_HANDLER(onPosition) },
    { "go", MAKE_COMMAND_HANDLER(onGo) },
    { "stop", MAKE_COMMAND_HANDLER(onStop) },
    { "ponderhit", MAKE_COMMAND_HANDLER(onPonderhit) },
    { "gameover", MAKE_COMMAND_HANDLER(onGameOver) },
  };

  for (;;) {
    auto command = receive();

    auto args = StringUtil::split(command, [](char c) {
      return isspace(c);
    });

    if (args.empty()) {
      Loggers::warning << "empty line is received.";
      continue;
    }

    if (args[0] == "quit") {
      stopSearchIfRunning();
      Loggers::message << "quit";
      return true;
    }

    auto ite = handlerMap.find(args[0]);
    if (ite == handlerMap.end()) {
      Loggers::warning << "unsupported command is received. '" << command << "'";
      continue;
    }

    const auto& handler = ite->second;
    bool ok = handler(args);
    if (!ok) {
      Loggers::warning << "fatal error occurred. '" << command << "'";
    }
  }
}

bool UsiClient::onIsReady(const CommandArguments&) {
  if (!checkStateIn(State::Ready)) {
    Loggers::warning << "invalid state: " << toString(state_);
    return false;
  }

  send("readyok");

  return true;
}

bool UsiClient::onSetOption(const CommandArguments&) {
  // TODO
 
  return true;
}

bool UsiClient::onUsiNewGame(const CommandArguments&) {
  if (!checkStateIn(State::Ready)) {
    Loggers::warning << "invalid state: " << toString(state_);
    return false;
  }

  // TODO
 
  return true;
}

bool UsiClient::onPosition(const CommandArguments& args) {
  if (!checkStateIn(State::Ready)) {
    Loggers::warning << "invalid state: " << toString(state_);
    return false;
  }

  size_t nextIndex;
  if (args.size() >= 2 && args[1] == "startpos") {
    position_.initialize(Position::Handicap::Even);
    nextIndex = 2;

  } else if (args.size() >= 6 && args[1] == "sfen") {
    bool ok = SfenParser::parsePosition(args[2], args[3], args[4], args[5], position_);
    if (!ok) {
      return false;
    }
    nextIndex = 6;

  } else {
    Loggers::error << "illegal arguments";
    return false;
  }

  positionIsInitialized_ = true;

  if (args.size() <= nextIndex) {
    return true;
  }

  if (args[nextIndex] != "moves") {
    Loggers::error << "illegal arguments";
    return false;
  }

  for (auto i = nextIndex + 1; i < args.size(); i++) {
    Move move;
    bool parseOk = SfenParser::parseMove(args[i], position_, move);
    if (!parseOk) {
      Loggers::error << "illegal arguments";
      return false;
    }

    bool moveOk = position_.doMove(move);
    if (!moveOk) {
      Loggers::error << "illegal move";
      return false;
    }
  }

  Loggers::message << position_;
 
  return true;
}

bool UsiClient::onGo(const CommandArguments& args) {
  if (!checkStateIn(State::Ready, State::Ponder)) {
    Loggers::warning << "invalid state: " << toString(state_);
    return false;
  }

  if (!positionIsInitialized_) {
    Loggers::error << "position command has not received.";
    return false;
  }

  if (args[1] == "ponder") {
    // TODO
    return true;
  }

  blackTimeMilliSeconds_ = 0;
  whiteTimeMilliSeconds_ = 0;
  byoyomiMilliSeconds_ = 0;
  isInfinite_ = false;

  for (size_t i = 1; i < args.size(); i++) {
    if (args[i] == "btime") {
      blackTimeMilliSeconds_ = strtol(args[++i].c_str(), nullptr, 10);

    } else if (args[i] == "wtime") {
      whiteTimeMilliSeconds_ = strtol(args[++i].c_str(), nullptr, 10);

    } else if (args[i] == "byoyomi") {
      byoyomiMilliSeconds_ = strtol(args[++i].c_str(), nullptr, 10);

    } else if (args[i] == "infinite") {
      isInfinite_ = true;

    } else if (args[i] == "mate") {
      Loggers::error << "mate option is not supported";
      return false;
    }
  }

  Loggers::message << "btime     = " << blackTimeMilliSeconds_;
  Loggers::message << "wtime     = " << whiteTimeMilliSeconds_;
  Loggers::message << "byoyomi   = " << byoyomiMilliSeconds_;
  Loggers::message << "inifinite = " << (isInfinite_ ? "true" : "false");

  stopSearchIfRunning();

  changeState(State::Search);

  searcherIsStarted_ = false;

  searchThread_.reset(new std::thread([this]() {
    search();
  }));

  waitForSearcherIsStarted();
 
  return true;
}

bool UsiClient::onStop(const CommandArguments&) {
  stopSearchIfRunning();

  if (isInfinite_) {
    sendBestMove();
  }
 
  return true;
}

void UsiClient::search() {
  Loggers::message << "search thread is started. tid=" << std::this_thread::get_id();

  int depth = 9;

  searcher_.idsearch(position_, depth * Searcher::Depth1Ply);

  if (!isInfinite_) {
    sendBestMove();
  }

  changeState(State::Ready);

  Loggers::message << "search thread is stopped. tid=" << std::this_thread::get_id();
}

void UsiClient::waitForSearcherIsStarted() {
  while (true) {
    std::this_thread::yield();
    if (searcherIsStarted_) { break; }
  }
}

void UsiClient::onStart() {
  searcherIsStarted_ = true;
}

void UsiClient::onUpdatePV(const PV& pv, float elapsed, int depth, Score score) {
  if (pv.size() == 0) {
    Loggers::warning << "PV is empty: " << __FILE__ << ':' << __LINE__;
    return;
  }

  auto& info = searcher_.getInfo();

  auto timeMilliSeconds = static_cast<uint32_t>(elapsed * 1e3);
  auto realDepth = depth / Searcher::Depth1Ply;
  auto nps = static_cast<uint32_t>(info.nodes / elapsed);

  const char* scoreKey;
  int scoreValue;
  if (score > -Score::mate() && score < Score::mate()) {
    scoreKey = "cp";
    scoreValue = score.raw() * 100.0 / material::Pawn;
  } else {
    scoreKey = "mate";
    if (score >= 0) {
      scoreValue = (Score::infinity() - score).raw();
    } else {
      scoreValue = -(Score::infinity() + score).raw();
    }
  }

  send("info",
       "time", timeMilliSeconds,
       "depth", realDepth,
       "nodes", info.nodes,
       "nps", nps,
       "currmove", pv.get(0).toStringSFEN(),
       "score", scoreKey, scoreValue,
       "pv", pv.toStringSFEN());
}

void UsiClient::onFailLow(const PV& pv, float elapsed, int depth, Score score) {
  onUpdatePV(pv, elapsed, depth, score);
  send("info", "string", "fail-low");
}

void UsiClient::onFailHigh(const PV& pv, float elapsed, int depth, Score score) {
  onUpdatePV(pv, elapsed, depth, score);
  send("info", "string", "fail-high");
}

void UsiClient::stopSearchIfRunning() {
  if (searchThread_.get() && searchThread_->joinable()) {
    Loggers::message << "stopping search thread..";
    searcher_.interrupt();
    searchThread_->join();
  }
}

void UsiClient::sendBestMove() {
  const auto& result = searcher_.getResult();

  if (!result.move.isEmpty()) {
    send("bestmove", result.move.toStringSFEN());
  } else {
    send("bestmove", "resign");
  }
}

bool UsiClient::onPonderhit(const CommandArguments&) {
  // TODO
 
  return true;
}

bool UsiClient::onGameOver(const CommandArguments&) {
  changeState(State::Ready);
 
  return true;
}

std::string UsiClient::receive() {
  std::string command;
  std::getline(std::cin, command);
  Loggers::receive << command;
  return command;
}

template <class T>
void UsiClient::send(T&& command) {
  std::lock_guard<std::mutex> lock(sendMutex_);

  std::cout << command << std::endl;
  Loggers::send << command;
}

template <class T, class... Args>
void UsiClient::send(T&& command, Args&&... options) {
  std::ostringstream oss;
  oss << command;
  joinOptions(oss, std::forward<Args>(options)...);
  send(oss.str());
}

template <class T>
void UsiClient::joinOptions(std::ostream& os, T&& arg) {
  os << ' ' << arg;
}

template <class T, class... Args>
void UsiClient::joinOptions(std::ostream& os, T&& arg, Args&&... args) {
  os << ' ' << arg;
  joinOptions(os, std::forward<Args>(args)...);
}

bool UsiClient::checkStateIn(State state) const {
  if (state_ == state) {
    return true;
  }
  return false;
}

template <class... Args>
bool UsiClient::checkStateIn(State state, Args... args) const {
  if (state_ == state) {
    return true;
  }
  return checkStateIn(args...);
}

void UsiClient::changeState(State state) {
  Loggers::message << "change state: " << toString(state_) << " => " << toString(state);
  state_ = state;
}

std::string UsiClient::toString(State state) {
  switch (state) {
  case State::None  : return "None";
  case State::Ready : return "Ready";
  case State::Ponder: return "Ponder";
  case State::Search: return "Search";
  case State::Mate  : return "Mate";
  }
  return "";
}

} // namespace sunfish
