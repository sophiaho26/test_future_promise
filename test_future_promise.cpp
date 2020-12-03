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

enum RC {
	kOK = 0,
	kTimeout,
	kFail
};

std::mutex m;
std::condition_variable cv;
bool finished = false;
using SATRT_COMPLETE_FUNC = std::function<void(const std::string&, const RC&, const State&)>;

void OnStartComplete(const std::string& name, const RC& rc, const State& state) {
	std::time_t start_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::cout << name << ": time now: " << std::ctime(&start_time) << std::endl;

	std::cout << name << ":rc = " << rc << std::endl;
	std::cout << name << ":state = " << state << std::endl;
	finished = true;
}

class StreamSession {
public:
	StreamSession(std::string name) { name_ = name; }
	~StreamSession() {}

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
	void Start(StreamSession* session, int timeout, SATRT_COMPLETE_FUNC funt_ptr) {
		if (state_ != kInited) return;
		state_ = kStarting;
		api_thread_ = std::thread(&StreamSession::StartTask, this, funt_ptr, timeout);
		return;
	}
	void StartCanTerminate(StreamSession* session, int timeout, SATRT_COMPLETE_FUNC funt_ptr) {
		if (state_ != kInited) return;
		state_ = kStarting;
		std::thread t = std::thread(&StreamSession::StartTaskCanTerminate, this, funt_ptr, timeout);
		t.join();
		return;
	}

	void UpdateTrackInfo(StreamSession* session, int timeout, SATRT_COMPLETE_FUNC funt_ptr) {
		if (state_ != kUpdateNeeded && state_ != kUnstable) return;
		api_thread_ = std::thread(&StreamSession::StartTaskCanTerminate, this, funt_ptr, timeout);
		return;
	}
private:
	void APIThreadStart() {
		do {

		} while (inited_);
	}
	void RunCreateChannel(std::promise<int> p) {
		std::cout << name_ << ":CreateChannel Thread Start" << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		// 	p1.set_value_at_thread_exit(1);
		p.set_value(2);

		ramdon = 2;
		std::cout << name_ << ":CreateChannel Thread End" << std::endl;
	}

	void RunCreateChannelCanTerminate(std::future<void> future_obj, std::promise<void> p) {
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
		p.set_value();
	}

	void RunSingalingConnectCanTerminate(std::future<void> future_obj, std::promise<void> p) {
		std::cout << name_ << ":RunSingalingConnectCanTerminate Thread Start" << std::endl;
		int i = 0;
		while (future_obj.wait_for(std::chrono::milliseconds(1)) == std::future_status::timeout && (++i < 7)) {
			std::this_thread::sleep_for(std::chrono::milliseconds(3000));
			std::cout << name_ << ":RunSingalingConnectCanTerminate" << std::endl;
		}

		// 	p1.set_value_at_thread_exit(6);
		ramdon = 6;
		std::cout << name_ << ":RunSingalingConnect Thread End" << std::endl;
		p.set_value();

	}
	void RunSingalingConnect(std::promise<int> p) {
		std::cout << name_ << ":RunSingalingConnect Thread Start" << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(3000));
		// 	p1.set_value_at_thread_exit(6);
		p.set_value(6);
		ramdon = 6;
		std::cout << name_ << ":RunSingalingConnect Thread End" << std::endl;

	}



	void StartTask(SATRT_COMPLETE_FUNC funt_ptr, int timeout) {
		RC rc = RC::kFail;
		std::chrono::system_clock::time_point time_out_seconds
			= std::chrono::system_clock::now() + std::chrono::seconds(timeout);

		auto start = std::chrono::system_clock::now();
		std::time_t start_time = std::chrono::system_clock::to_time_t(start);
		std::cout << name_ << ":time now: " << std::ctime(&start_time) << std::endl;

		std::promise<int> p1;
		std::future<int> f_completes = p1.get_future();

		std::thread th1_ = std::thread(&StreamSession::RunCreateChannel, this, std::move(p1));
		th1_.detach();
		// 		std::thread([](std::promise<int> p1)
		// 			{
		// 				std::cout << "CreateChannel Thread Start" << std::endl;
		// 				std::this_thread::sleep_for(std::chrono::seconds(1));
		// 				// 	p1.set_value_at_thread_exit(9);
		// 				p1.set_value(5);
		// 				std::cout << "CreateChannel Thread End" << std::endl;
		// 			},
		// 			std::move(p1)
		// 				).detach();


		if (std::future_status::ready == f_completes.wait_until(time_out_seconds))
		{
			std::cout << name_ << ":RunCreateChannel: " << f_completes.get() << " Completed.\n";
		}
		else
		{
			std::cout << name_ << ":RunCreateChannel did not complete!\n";
			rc = RC::kTimeout;
			funt_ptr(name_, rc, state_);
			return;
		}

		std::promise<int> p2;
		std::future<int> f_times_out = p2.get_future();

		// 			std::thread([](std::promise<int> p2)
		// 				{
		// 					std::cout << "RunSingalingConnect Thread Start" << std::endl;
		// 					std::this_thread::sleep_for(std::chrono::seconds(2));
		// 					// 	p1.set_value_at_thread_exit(9);
		// 					p2.set_value(3);
		// 					std::cout << "RunSingalingConnect Thread End" << std::endl;
		// 				},
		// 				std::move(p2)
		// 					).detach();

		std::thread th2_ = std::thread(&StreamSession::RunSingalingConnect, this, std::move(p2));
		th2_.detach();
		if (std::future_status::ready == f_times_out.wait_until(time_out_seconds))
		{
			std::cout << name_ << ":RunSingalingConnect: " << f_times_out.get() << " Completed.\n";
		}
		else
		{
			std::cout << name_ << ":RunSingalingConnect did not complete!\n";
			rc = RC::kTimeout;
			funt_ptr(name_, rc, state_);
			return;
		}

		rc = RC::kOK;
		state_ = KReady;
		funt_ptr(name_, rc, state_);
		return;
	}

	void StartTaskCanTerminate(SATRT_COMPLETE_FUNC funt_ptr, int timeout) {
		RC rc = RC::kFail;
		std::chrono::system_clock::time_point time_out_seconds
			= std::chrono::system_clock::now() + std::chrono::seconds(timeout);

		auto start = std::chrono::system_clock::now();
		std::time_t start_time = std::chrono::system_clock::to_time_t(start);
		std::cout << name_ << ":time now: " << std::ctime(&start_time) << std::endl;

		std::promise<void> exit_signal_1;
		std::future<void> exit_future_1 = exit_signal_1.get_future();
		std::promise<void> execute_promise_1;
		std::future<void> execute_future_1 = execute_promise_1.get_future();

		std::unique_ptr<std::thread>th1_ = std::make_unique<std::thread>(&StreamSession::RunCreateChannelCanTerminate, this, std::move(exit_future_1), std::move(execute_promise_1));

		if (th1_->joinable()) {
			th1_->join();
		}

		// 		std::thread([](std::promise<int> p1)
		// 			{
		// 				std::cout << "CreateChannel Thread Start" << std::endl;
		// 				std::this_thread::sleep_for(std::chrono::seconds(1));
		// 				// 	p1.set_value_at_thread_exit(9);
		// 				p1.set_value(5);
		// 				std::cout << "CreateChannel Thread End" << std::endl;
		// 			},
		// 			std::move(p1)
		// 				).detach();


		if (std::future_status::ready == execute_future_1.wait_until(time_out_seconds))
		{
			std::cout << name_ << ":RunCreateChannel Finished: \n";
		}
		else
		{
			std::cout << name_ << ":RunCreateChannel did not complete!\n";
			rc = RC::kTimeout;
			exit_signal_1.set_value();
			// 			th1_.reset();
			funt_ptr(name_, rc, state_);
			// 			if (th1_.joinable()) {
			// 				th1_.join();
			// 			}
			return;
		}
		// 		if (th1_.joinable()) {
		// 			th1_.join();
		// 		}

		std::promise<void> exit_signal_2;
		std::future<void> exit_future_2 = exit_signal_2.get_future();
		std::promise<void> execute_promise_2;
		std::future<void> execute_future_2 = execute_promise_2.get_future();

		// 			std::thread([](std::promise<int> p2)
		// 				{
		// 					std::cout << "RunSingalingConnect Thread Start" << std::endl;
		// 					std::this_thread::sleep_for(std::chrono::seconds(2));
		// 					// 	p1.set_value_at_thread_exit(9);
		// 					p2.set_value(3);
		// 					std::cout << "RunSingalingConnect Thread End" << std::endl;
		// 				},
		// 				std::move(p2)
		// 					).detach();

		std::unique_ptr<std::thread> th2_ = std::make_unique<std::thread>(&StreamSession::RunSingalingConnectCanTerminate, this, std::move(exit_future_2), std::move(execute_promise_2));

		// 		if (th2_->joinable()) {
		// 			th2_->join();
		// 		}

		if (std::future_status::ready == execute_future_2.wait_until(time_out_seconds))
		{
			std::cout << name_ << ":RunSingalingConnect: Finished: " << "\n";
		}
		else
		{
			std::cout << name_ << ":RunSingalingConnect did not complete!\n";
			rc = RC::kTimeout;
			exit_signal_2.set_value();
			if (th2_->joinable()) {
				th2_->join();
			}
			th2_.reset();
			funt_ptr(name_, rc, state_);
			return;
		}
		if (th2_->joinable()) {
			th2_->join();
		}

		rc = RC::kOK;
		state_ = KReady;
		funt_ptr(name_, rc, state_);
		return;
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


	StreamSession s1("s1");
	s1.Init();
	StreamSession s2("s2");
	s2.Init();

	RC rc;
	State state;

	s1.StartCanTerminate(&s1, wait_seconds, &OnStartComplete);
	auto lambda = [&s1, &wait_seconds](StreamSession s1, int wait_seconds) {
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
	wait_seconds = 4;
	start = std::chrono::system_clock::now();
	time_out_seconds
		= start + std::chrono::seconds(wait_seconds);
	s2.StartCanTerminate(&s2, 2, &OnStartComplete);
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