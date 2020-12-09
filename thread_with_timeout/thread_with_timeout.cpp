#include <chrono>
#include <ctime>
#include <future>
#include <iostream>
#include <thread>
#define _CRT_SECURE_NO_WARNINGS

class ChannelService {
 public:
  ChannelService() {}
  void CreateChannel() {}
};

enum State {
  kInactive = 0,
  kInited,
  kStarting,
  KReady,
  kUpdateNeeded,
  kUpdating,
  kStopping,
  kUnstable
};

enum common { kAudienceOk = 0, kTimeout, kFail };

std::mutex m;
std::condition_variable cv;
using SATRT_COMPLETE_FUNC =
    std::function<void(const std::string&, const common&, const State&)>;

void OnStartComplete(const std::string& name, const common& rc,
                     const State& state) {
  std::time_t start_time =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::cout << name << ": time now: " << std::ctime(&start_time) << std::endl;
  std::cout << name << ":rc = " << rc << std::endl;
  std::cout << name << ":state = " << state << std::endl;
}

struct ExitAndExecuteSignals {
  std::promise<void> exit_signal;
  std::future<void> exit_future = exit_signal.get_future();
  std::promise<common> execute_signal;
  std::future<common> execute_future = execute_signal.get_future();
};

enum class StartState {
  kDefault = 0,
  kChannelCreated,
  kSignalingConnected,
  kTrackUpdated
};

enum class SIGNALING_CONNECTION_STATE { kClosed = 0, kOpened, kFailed };

template <typename Func, typename Obj>
common WailUntil(Func func, Obj obj, std::string name,
                 std::chrono::system_clock::time_point& timeout_time) {
  auto rc = common::kAudienceOk;
  ExitAndExecuteSignals signals;

  std::unique_ptr<std::thread> th = std::make_unique<std::thread>(
      func, obj, std::move(signals.exit_future),
      std::move(signals.execute_signal), timeout_time);

  if (std::future_status::ready ==
      signals.execute_future.wait_until(timeout_time)) {
    std::cout << name << ":Run Func Finished. \n";
  } else {
    std::cout << name << ":Run Func did not complete!\n";
    rc = common::kTimeout;
    signals.exit_signal.set_value();
    if (th->joinable()) {
      th->join();
    }
    return rc;
  }
  if (th->joinable()) {
    th->join();
  }

  rc = signals.execute_future.get();
  if (rc != common::kAudienceOk) {
    std::cout << name << ":Func return rc: " << rc << " \n";
  }
  return rc;
}

template <typename Func, typename Obj>
common RunUntil(Func func, Obj obj, std::string name,
                std::chrono::system_clock::time_point& timeout_time) {
  auto rc = common::kAudienceOk;

  ExitAndExecuteSignals signals;
  std::promise<common> promise;
  std::future<common> future = promise.get_future();
  std::atomic<bool> timeout = false;
  std::mutex cv_m;
  std::condition_variable cv;

  std::unique_ptr<std::thread> th = std::make_unique<std::thread>(
      func, obj, cv_m, cv, std::move(promise), timeout_time);

  if (std::future_status::ready == future.wait_until(timeout_time)) {
    std::cout << name << ":Run Func Finished. \n";
  } else {
    std::cout << name << ":Run Func did not complete!\n";
    rc = common::kTimeout;
    std::unique_lock<std::mutex> lk(cv_m);
    timeout = true;
    lk.unlock();
    cv.notify_all();
    if (th->joinable()) {
      th->join();
    }
    return rc;
  }
  if (th->joinable()) {
    th->join();
  }

  rc = future.get();
  if (rc != common::kAudienceOk) {
    std::cout << name << ":Func return rc: " << rc << " \n";
  }
  return rc;
}

class BaseStreamSession {
 public:
  BaseStreamSession(std::string name) { name_ = name; }
  ~BaseStreamSession() { 
    if (api_thread_.joinable()) {
      api_thread_.join();
    }
  }

  void Init() {
    if (state_ != kInactive) return;
    inited_ = true;
    state_ = kInited;
  }
  void Reset() {
    if (state_ != kInited) return;
    inited_ = false;
    state_ = kInactive;
    if (api_thread_.joinable()) {
      api_thread_.join();
    }
  }
  void Start(BaseStreamSession* session, int timeout,
             SATRT_COMPLETE_FUNC funt_ptr) {
    if (state_ != kInited) {
      if (state_ == kStarting) {
        std::cout << "already starting...\n";
      }
      return;
    }
    state_ = kStarting;
    api_thread_ = std::thread(&BaseStreamSession::RunStartNew, this, funt_ptr, timeout);
    return;
  }
  void Stop(BaseStreamSession* session, int timeout,
      SATRT_COMPLETE_FUNC funt_ptr) {
    if (state_ != KReady) return;
    state_ = kStopping;
    std::thread t = std::move(api_thread_);
    if (t.joinable()) {
      t.join();
    }

    api_thread_ = std::thread(&BaseStreamSession::RunStartNew, this, funt_ptr,
                            timeout);
    return;

  }
 private:
  common CreateChannel(std::chrono::system_clock::time_point& timeout_time) {
    std::cout << name_ << ":CreateChannelID entered" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    std::cout << name_ << ":CreateChannelID left" << std::endl;
    return common::kAudienceOk;
  }

  common ConnectSignalingServer(
      std::chrono::system_clock::time_point& timeout_time) {
    std::cout << name_ << ":ConnectSignalingServer entered" << std::endl;
    std::thread([&]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(600));
      std::lock_guard<std::mutex> lk(signaling_mutex_);
      signaling_state_ = SIGNALING_CONNECTION_STATE::kOpened;
      signaling_cv.notify_all();
    }).detach();

    std::unique_lock<std::mutex> lk(signaling_mutex_);
    signaling_cv.wait_until(lk, timeout_time, [this] {
      return (signaling_state_ == SIGNALING_CONNECTION_STATE::kOpened);
    });
    lk.unlock();
    if (signaling_state_ != SIGNALING_CONNECTION_STATE::kOpened) {
      std::cout << name_ << ":ConnectSignalingServer timeout" << std::endl;
      std::cout.flush();
      return common::kTimeout;
    }

    std::cout << name_ << ":ConnectSignalingServer left" << std::endl;
    return common::kAudienceOk;
  }

  common UpdateChannelID(std::chrono::system_clock::time_point& timeout_time) {
    std::cout << name_ << ":UpdateChannelID entered" << std::endl;
    std::thread([&]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      std::lock_guard<std::mutex> lk(update_id_mutex_);
      update_id = true;
      update_id_cv.notify_all();
    }).detach();

    std::unique_lock<std::mutex> lk(update_id_mutex_);
    update_id_cv.wait_until(lk, timeout_time, [this] { return update_id; });
    lk.unlock();
    if (!update_id) {
      std::cout << name_ << ":UpdateChannelID timeout" << std::endl;
      return common::kTimeout;
    }

    std::cout << name_ << ":UpdateChannelID left" << std::endl;
    return common::kAudienceOk;
  }
  void RunRollBack() {
  }

  common RunStartNew(SATRT_COMPLETE_FUNC funt_ptr, int timeout) {
    auto rc = common::kTimeout;
    state_ = kStarting;
    std::chrono::system_clock::time_point timeout_time =
        std::chrono::system_clock::now() + std::chrono::seconds(timeout);

    if (start_state_ == StartState::kDefault) {
      rc = CreateChannel(timeout_time);
      if (rc != common::kAudienceOk) {
        std::cout << "CreateChannelID rc ({})" << rc;
      } else {
        start_state_ = StartState::kChannelCreated;
      }
    }
    if (start_state_ == StartState::kChannelCreated) {
      rc = ConnectSignalingServer(timeout_time);
      if (rc != common::kAudienceOk) {
        std::cout << "ConnectSignalingServer rc ({})" << rc;
      } else {
        start_state_ = StartState::kSignalingConnected;
      }
    }
    if (start_state_ == StartState::kSignalingConnected) {
      rc = UpdateChannelID(timeout_time);
      if (rc != common::kAudienceOk) {
        std::cout << "UpdateChannelID rc ({})" << rc;
      } else {
        start_state_ = StartState::kTrackUpdated;
        state_ = KReady;
      }
    }
    funt_ptr(name_, rc, state_);
    return rc;
  }

  void RunCreateChannelCanTerminate(std::future<void> future_obj,
                                    std::promise<common> p) {
    std::cout << name_ << ":RunCreateChannelCanTerminate Thread Start"
              << std::endl;
    int i = 0;
    while (future_obj.wait_for(std::chrono::milliseconds(1)) ==
               std::future_status::timeout &&
           (++i < 3)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      std::cout << name_ << ":RunCreateChannelCanTerminate" << std::endl;
    }
    // 	p1.set_value_at_thread_exit(2);
    ramdon = 2;
    std::cout << name_ << ":RunCreateChannelCanTerminate Thread End"
              << std::endl;
    std::cout.flush();
    if (i == 3)
      p.set_value(common::kAudienceOk);
    else
      p.set_value(common::kTimeout);
  }

  void RunSingalingConnectCanTerminate(std::future<void> future_obj,
                                       std::promise<common> p) {
    std::cout << name_ << ":RunSingalingConnectCanTerminate Thread Start"
              << std::endl;
    int i = 0;
    while (future_obj.wait_for(std::chrono::milliseconds(1)) ==
               std::future_status::timeout &&
           (++i < 7)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      std::cout << name_ << ":RunSingalingConnectCanTerminate" << std::endl;
    }

    // 	p1.set_value_at_thread_exit(6);
    ramdon = 6;
    std::cout << name_ << ":RunSingalingConnect Thread End" << std::endl;
    std::cout.flush();

    if (i == 7)
      p.set_value(common::kAudienceOk);
    else
      p.set_value(common::kTimeout);
  }

  bool inited_{false};
  std::thread api_thread_;
  State state_{State::kInactive};
  std::string name_{};
  int ramdon{0};
  StartState start_state_{StartState::kDefault};
  std::condition_variable signaling_cv;
  std::condition_variable update_id_cv;
  bool update_id = {false};
  mutable std::mutex signaling_mutex_ = {};
  mutable std::mutex update_id_mutex_ = {};
  std::atomic<SIGNALING_CONNECTION_STATE> signaling_state_{
      SIGNALING_CONNECTION_STATE::kClosed};
};

int main() {
  // stream session version
  auto start = std::chrono::system_clock::now();
  std::time_t start_time = std::chrono::system_clock::to_time_t(start);
  int wait_seconds = 3;
  int time_out_seconds = 1;

  std::cout << "time now: " << std::ctime(&start_time) << std::endl;

  BaseStreamSession s1("s1");
  s1.Init();
  BaseStreamSession s2("s2");
  s2.Init();

  common rc;
  State state;

  s1.Start(&s1, wait_seconds, &OnStartComplete);
  s1.Start(&s1, wait_seconds, &OnStartComplete);

  //   auto lambda = [&s1, wait_seconds](BaseStreamSession &s1, int wait_seconds) {
//     s1.Start(&s1, wait_seconds, &OnStartComplete);
//   };

  s2.Start(&s2, time_out_seconds, &OnStartComplete);
  s1.Stop(&s1, 1, &OnStartComplete);
  s2.Stop(&s2, 1, &OnStartComplete);
  s1.Reset();
  s2.Reset();
  std::cout << "Test Finished..\n";
}