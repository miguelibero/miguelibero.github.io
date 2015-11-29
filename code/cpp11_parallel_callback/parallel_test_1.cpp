#include <future>
#include <atomic>
#include <memory>
#include <iostream>
#include <cassert>

/**
 * g++ -W -Wall -Wextra -pedantic -std=c++0x -pthread -o parallel_test_1 parallel_test_1.cpp
 */

typedef std::function<void()> Callback;

void asyncCall(const Callback& callback)
{
	std::async(std::launch::async, [callback](){
		callback();
	});
}

void startTaskOne(const Callback& callback)
{
	asyncCall(callback);
}

void startTaskTwo(const Callback& callback)
{
	asyncCall(callback);
}

void startTwoTasks(const Callback& callback)
{
	auto counter = std::make_shared<std::atomic<int>>(2);
	Callback parallel([counter, callback](){
		--(*counter);
		if(counter->load() == 0)
		{
			callback();
		}
	});

	startTaskOne(parallel);
	startTaskTwo(parallel);
}

int main()
{
	std::cout << "started" << std::endl;
	startTwoTasks([](){
		std::cout << "finished" << std::endl;
	});
	return 0;
}