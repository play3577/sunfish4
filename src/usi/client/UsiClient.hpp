/* UsiClient.hpp
 *
 * Kubo Ryosuke
 */

#ifndef SUNFISH_USI_CLIENT_USICLIENT_HPP__
#define SUNFISH_USI_CLIENT_USICLIENT_HPP__

#include "core/position/Position.hpp"
#include "core/record/Record.hpp"
#include "search/Searcher.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <queue>
#include <cstdint>

namespace sunfish {

class UsiClient : public SearchHandler {
private:

  using CommandArguments = std::vector<std::string>;

  using TimeType = SearchConfig::TimeType;

  struct Options {
    bool ponder;
  };

  enum class CommandState : uint8_t {
    Ok,
    Eof,
    Error,
    Broken,
  };

  struct Command {
    CommandState state;
    std::string value;
  };

public:

  UsiClient();

  bool start();

private:

  bool acceptUsi();

  bool ready();

  bool setOption(const CommandArguments&);

  bool receiveNewGame();

  bool game();

  bool receiveGo();

  bool runSearch(const CommandArguments& args);

  void search();

  bool runPonder(const CommandArguments& args);

  void ponder();

  void waitForSearcherIsStarted();

  void waitForStopCommand();

  void onStart(const Searcher&) override;

  void onUpdatePV(const Searcher& searcher, const PV& pv, float elapsed, int depth, Score score) override;

  void onFailLow(const Searcher& searcher, const PV& pv, float elapsed, int depth, Score score) override;

  void onFailHigh(const Searcher& searcher, const PV& pv, float elapsed, int depth, Score score) override;

  Command receive();

  void receiver();

  void breakReceive();

  template <class T>
  void send(T&& command);

  template <class T, class... Args>
  void send(T&& command, Args&&... options);

  template <class T>
  void joinOptions(std::ostream& os, T&& arg);

  template <class T, class... Args>
  void joinOptions(std::ostream& os, T&& arg, Args&&... args);

  Options options_;

  std::queue<std::string> deferredCommands_;
  std::queue<Command> commandQueue_;

  std::string lastPositionCommand_;
  std::string lastGoCommand_;

  Record record_;

  TimeType blackMilliSeconds_;
  TimeType whiteMilliSeconds_;
  TimeType byoyomiMilliSeconds_;
  bool isInfinite_;

  Searcher searcher_;
  std::atomic<bool> searcherIsStarted_;
  std::atomic<bool> stopCommandReceived_;
  std::atomic<bool> breakReceiver_;

  std::mutex sendMutex_;
  std::mutex receiveMutex_;
  std::thread receiver_;

};

} // namespace sunfish

#endif // SUNFISH_USI_CLIENT_USICLIENT_HPP__
