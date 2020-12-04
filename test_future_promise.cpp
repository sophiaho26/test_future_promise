#include <iostream>
#include <future>
#include <thread>
#include <chrono>
#include <ctime>
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

enum common {
	kAudienceOk = 0,
	kTimeout,
	kFail
};

std::mutex m;
std::condition_variable cv;
bool finished = false;
using SATRT_COMPLETE_FUNC = std::function<void(const std::string&, const common&, const State&)>;

void OnStartComplete(const std::string& name, const common& rc, const State& state) {
	std::time_t start_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::cout << name << ": time now: " << std::ctime(&start_time) << std::endl;
	std::cout << name << ":rc = " << rc << std::endl;
	std::cout << name << ":state = " << state << std::endl;
	finished = true;
}

struct ExitAndExecuteSignals {
	std::promise<void> exit_signal;
	std::future<void> exit_future = exit_signal.get_future();
	std::promise<common> execute_signal;
	std::future<common> execute_future = execute_signal.get_future();
};


template <typename Func, typename Obj>
common WailUntil(Func func, Obj obj,std::string name,
	std::chrono::system_clock::time_point& time_out_seconds) {
	auto rc = common::kAudienceOk;
	ExitAndExecuteSignals signals;

	std::unique_ptr<std::thread>th1_ = std::make_unique<std::thread>(func, obj, std::move(signals.exit_future), std::move(signals.execute_signal));

	if (std::future_status::ready == signals.execute_future.wait_until(time_out_seconds))
	{
		std::cout << name << ":Run Func Finished. \n";
	}
	else
	{
		std::cout << name << ":Run Func did not complete!\n";
		rc = common::kTimeout;
		signals.exit_signal.set_value();
		if (th1_->joinable()) {
			th1_->join();
		}
		return rc;
	}
	if (th1_->joinable()) {
		th1_->join();
	}

	rc = signals.execute_future.get();
	if (rc != common::kAudienceOk) {
		std::cout << name << ":Func return rc: " << rc << " \n";
	}
	return rc;
}


class BaseStreamSession {
public:
	BaseStreamSession(std::string name) { name_ = name; }
	~BaseStreamSession() {}

	void Init() {
		if (state_ != kInactive) return;
		inited_ = true;
		state_ = kInited;
	}
	void Reset() {
		if (state_ != kInited) return;
		inited_ = false;
		state_ = kInactive;
		if (api_thread_.joinable())
		{
			api_thread_.join();
		}
	}
	void StartCanTerminate(BaseStreamSession* session, int timeout, SATRT_COMPLETE_FUNC funt_ptr) {
		if (state_ != kInited) return;
		state_ = kStarting;
		std::thread t = std::thread(&BaseStreamSession::StartTaskCanTerminate, this, funt_ptr, timeout);
		t.join();
		return;
	}

	void UpdateTrackInfo(BaseStreamSession* session, int timeout, SATRT_COMPLETE_FUNC funt_ptr) {
		if (state_ != kUpdateNeeded && state_ != kUnstable) return;
		api_thread_ = std::thread(&BaseStreamSession::StartTaskCanTerminate, this, funt_ptr, timeout);
		return;
	}
private:
	void RunCreateChannelCanTerminate(std::future<void> future_obj, std::promise<common> p) {
		std::cout << name_ << ":RunCreateChannelCanTerminate Thread Start" << std::endl;
		int i = 0;
		while (future_obj.wait_for(std::chrono::milliseconds(1)) == std::future_status::timeout && (++i < 3)) {
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			std::cout << name_ << ":RunCreateChannelCanTerminate" << std::endl;
		}
		// 	p1.set_value_at_thread_exit(2);
		ramdon = 2;
		std::cout << name_ << ":RunCreateChannelCanTerminate Thread End" << std::endl;
		std::cout.flush();
		if (i == 3)
			p.set_value(common::kAudienceOk);
		else
			p.set_value(common::kTimeout);
	}

	void RunSingalingConnectCanTerminate(std::future<void> future_obj, std::promise<common> p) {
		std::cout << name_ << ":RunSingalingConnectCanTerminate Thread Start" << std::endl;
		int i = 0;
		while (future_obj.wait_for(std::chrono::milliseconds(1)) == std::future_status::timeout && (++i < 7)) {
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
	void RunSingalingConnect(std::promise<int> p) {
		std::cout << name_ << ":RunSingalingConnect Thread Start" << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(3000));
		// 	p1.set_value_at_thread_exit(6);
		p.set_value(6);
		ramdon = 6;
		std::cout << name_ << ":RunSingalingConnect Thread End" << std::endl;

	}

	void StartTaskCanTerminate(SATRT_COMPLETE_FUNC funt_ptr, int timeout) {
		common rc = common::kAudienceOk;
		std::chrono::system_clock::time_point time_out_seconds
			= std::chrono::system_clock::now() + std::chrono::seconds(timeout);

		auto start = std::chrono::system_clock::now();
		std::time_t start_time = std::chrono::system_clock::to_time_t(start);
		std::cout << name_ << ":time now: " << std::ctime(&start_time) << std::endl;

// 		rc = CreatStreamChannelTask(time_out_seconds, funt_ptr);
		rc = WailUntil(&BaseStreamSession::RunCreateChannelCanTerminate, this, name_, time_out_seconds);
		if (rc != kAudienceOk) return;

		rc = WailUntil(&BaseStreamSession::RunSingalingConnectCanTerminate, this, name_, time_out_seconds);
// 		rc = ConnectAndUpdateToSignalingServerTask(time_out_seconds, funt_ptr);
		if (rc != kAudienceOk) { 
			std::cout << name_ << ":rc: " << rc << std::endl;
			return; }
	}


	common CreatStreamChannelTask(
		std::chrono::system_clock::time_point &time_out_seconds, SATRT_COMPLETE_FUNC funt_ptr) {
		auto rc = common::kAudienceOk;
		ExitAndExecuteSignals create_channel_signals;
		std::unique_ptr<std::thread>th1_ = std::make_unique<std::thread>(&BaseStreamSession::RunCreateChannelCanTerminate, this, std::move(create_channel_signals.exit_future), std::move(create_channel_signals.execute_signal));

		if (std::future_status::ready == create_channel_signals.execute_future.wait_until(time_out_seconds))
		{
			std::cout << name_ << ":RunCreateChannelCanTerminate Finished: \n";
		}
		else
		{
			std::cout << name_ << ":RunCreateChannelCanTerminate did not complete!\n";
			rc = common::kTimeout;
			create_channel_signals.exit_signal.set_value();
			if (th1_->joinable()) {
				th1_->join();
			}
			funt_ptr(name_, rc, state_);
			return rc;
		}
		if (th1_->joinable()) {
			th1_->join();
		}

		rc = create_channel_signals.execute_future.get();
		if (rc != common::kAudienceOk) {
			std::cout << name_ << ":RunCreateChannel Fail \n";
		}
		return rc;
	}

	common ConnectAndUpdateToSignalingServerTask(
		std::chrono::system_clock::time_point time_out_seconds, SATRT_COMPLETE_FUNC funt_ptr) {
		auto rc = common::kAudienceOk;
		ExitAndExecuteSignals connect_signaling_signals;

		std::unique_ptr<std::thread> th2_ = std::make_unique<std::thread>(&BaseStreamSession::RunSingalingConnectCanTerminate, this, std::move(connect_signaling_signals.exit_future), std::move(connect_signaling_signals.execute_signal));
// 		if (th2_->joinable()) {
// 			th2_->join();
// 		}

		if (std::future_status::ready == connect_signaling_signals.execute_future.wait_until(time_out_seconds))
		{
			std::cout << name_ << ":RunSingalingConnect: Finished: " << "\n";
		}
		else
		{
			std::cout << name_ << ":RunSingalingConnect did not complete!\n";
			rc = common::kTimeout;
			connect_signaling_signals.exit_signal.set_value();

			if (th2_->joinable()) {
				th2_->join();
				}
			funt_ptr(name_, rc, state_);
			return rc;
		}
		if (th2_->joinable()) {
			th2_->join();
		}

		rc = connect_signaling_signals.execute_future.get();
		if (rc != common::kAudienceOk) {
			std::cout << name_ << ":RunSingalingConnect Failed \n";
		}
		funt_ptr(name_, rc, state_);
		return rc;
	}

	bool inited_{ false };
	std::thread api_thread_;
	State state_{ State::kInactive };
	std::string name_{};
	int ramdon{ 0 };
};

int main()
{
	// stream session version
	auto start = std::chrono::system_clock::now();
	std::time_t start_time = std::chrono::system_clock::to_time_t(start);
	int wait_seconds = 5;
	std::chrono::system_clock::time_point time_out_seconds
		= start + std::chrono::seconds(wait_seconds);

	std::cout << "time now: " << std::ctime(&start_time) << std::endl;
	std::cout << "end time: " << std::chrono::system_clock::to_time_t(time_out_seconds) << std::endl;


	BaseStreamSession s1("s1");
	s1.Init();
	BaseStreamSession s2("s2");
	s2.Init();

	common rc;
	State state;

	s1.StartCanTerminate(&s1, wait_seconds, &OnStartComplete);
	auto lambda = [&s1, &wait_seconds](BaseStreamSession s1, int wait_seconds) {
		s1.StartCanTerminate(&s1, wait_seconds, &OnStartComplete);
	};

	// 		s1.Start(&s1, wait_seconds, &OnStartComplete);
			//Wait for 4 sec
	std::unique_lock<std::mutex> lk(m);
	if (cv.wait_until(lk, time_out_seconds, []() {return finished == true; })) {
		std::cout << "s1 Finished\n";
	}
	else {
		std::cout << "s1 No Reply and time out\n";
	}

	start_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::cout << "time now: " << std::ctime(&start_time) << std::endl;

	finished = false;
	wait_seconds = 3;
	start = std::chrono::system_clock::now();
	time_out_seconds
		= start + std::chrono::seconds(wait_seconds);
	s2.StartCanTerminate(&s2, 1, &OnStartComplete);
	// 		s2.Start(&s2, 2, &OnStartComplete);

	if (cv.wait_until(lk, time_out_seconds, []() {return finished == true; })) {
		std::cout << "s2 Finished\n";
	}
	else {
		std::cout << "s2 No Reply and time out\n";
	}
	start_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::cout << "time now: " << std::ctime(&start_time) << std::endl;

	s1.Reset();
	s2.Reset();
	std::cout << "Test Finished..\n";
}