#include <iostream>
#include <future>
#include <thread>
#include <chrono>
#include <ctime>

void threadFunction(std::promise<int> p1)
{
	std::cout << "Thread Start" << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(1));
	// 	p1.set_value_at_thread_exit(9);
	p1.set_value(9);

	std::cout << "Thread End" << std::endl;
}


int main()
{

	std::chrono::system_clock::time_point time_out_seconds
		= std::chrono::system_clock::now() + std::chrono::seconds(2);

	// Make a future that takes 1 second to complete
	std::promise<int> p1;
	std::future<int> f_completes = p1.get_future();
	std::thread th(threadFunction, std::move(p1));
	// Make a future that takes 5 seconds to complete
	std::promise<int> p2;
	std::future<int> f_times_out = p2.get_future();
	std::thread([](std::promise<int> p2)
		{
			std::this_thread::sleep_for(std::chrono::seconds(5));
			// 				p2.set_value_at_thread_exit(8);
			p2.set_value(8);
		},
		std::move(p2)
			).detach();

		auto start = std::chrono::system_clock::now();
		std::time_t start_time = std::chrono::system_clock::to_time_t(start);
		std::cout << "time now: " << std::ctime(&start_time) << std::endl;
		std::cout << "Waiting for 2 seconds..." << std::endl;

		if (std::future_status::ready == f_completes.wait_until(time_out_seconds))
		{
			std::cout << "f_completes: " << f_completes.get() << "\n";
		}
		else
		{
			std::cout << "f_completes did not complete!\n";
		}

		auto end1 = std::chrono::system_clock::now();
		std::time_t end_time_1 = std::chrono::system_clock::to_time_t(end1);
		std::chrono::duration<double> elapsed_seconds = end1 - start;
		std::cout << "time now: " << std::ctime(&end_time_1) << std::endl;
		std::cout << "elapsed time: " << elapsed_seconds.count() << std::endl;

		if (std::future_status::ready == f_times_out.wait_until(time_out_seconds))
		{
			std::cout << "f_times_out: " << f_times_out.get() << "\n";
		}
		else
		{
			std::cout << "f_times_out did not complete!\n";
		}
		auto end2 = std::chrono::system_clock::now();
		std::time_t end_time_2 = std::chrono::system_clock::to_time_t(end2);
		std::chrono::duration<double> elapsed_seconds_2 = end2 - start;
		std::cout << "time now: " << std::ctime(&end_time_2) << std::endl;
		std::cout << "elapsed time: " << elapsed_seconds_2.count() << std::endl;

		th.detach();

		std::cout << "Done!\n";
}