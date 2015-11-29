#include <future>
#include <atomic>
#include <memory>
#include <iostream>
#include <cassert>

/**
 * g++ -W -Wall -Wextra -pedantic -std=c++0x -pthread -o parallel_test_2 parallel_test_2.cpp
 */

typedef std::function<void()> Callback;

struct ParallelCallbackData
{
	Callback callback;
	std::atomic<int> counter;
    std::atomic<bool> started;

    ParallelCallbackData(const Callback& callback):
    callback(callback), counter(0), started(false)
    {
    }
};

class ParallelCallback
{
private:
	typedef ParallelCallbackData Data;
	typedef std::shared_ptr<Data> DataPtr;
    DataPtr _data;

    static void step(const DataPtr& data)
    {
    	if(data->counter-- == 0)
    	{
    		if(data->started && data->callback)
    		{
    			data->callback();
    		}
    	}
    }

public:

	ParallelCallback(const Callback& callback) :
	_data(new Data(callback))
	{
	}

    operator Callback()
    {
        assert(!_data->started);
        ++_data->counter;
        return std::bind(&ParallelCallback::step, _data);
    }

    void check()
    {
        assert(!_data->started);
        _data->started = true;
        step(_data);
    }

};


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
    ParallelCallback parallel(callback);
	startTaskOne(parallel);
	startTaskTwo(parallel);
    parallel.check();
}

int main()
{
	std::cout << "started" << std::endl;
	startTwoTasks([](){
		std::cout << "finished" << std::endl;
	});
	return 0;
}