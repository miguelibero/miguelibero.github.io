#include <future>
#include <atomic>
#include <memory>
#include <iostream>
#include <cassert>

/**
 * g++ -W -Wall -Wextra -pedantic -std=c++0x -pthread -o parallel_test_3 parallel_test_3.cpp
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

    template<class Arg, class... Args>
    void operator ()(Arg&& arg, Args&&... args)
    {
        arg(*this);
        operator ()(args...);
    }

    template<class Arg>
    void operator ()(Arg&& arg)
    {
        arg(*this);
        check();
    }

    template<class Arg, class... Args>
    static void run(Arg&& arg, Args&&... args)
    {
        ParallelCallback parallel(arg);
        parallel(args...);
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
    ParallelCallback::run(callback, startTaskOne, startTaskTwo);
}

int main()
{
	std::cout << "started" << std::endl;
	startTwoTasks([](){
		std::cout << "finished" << std::endl;
	});
	return 0;
}