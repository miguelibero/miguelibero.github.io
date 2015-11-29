---
layout: post
title:  "C++11 async parallel callback"
date:   2014-05-25 21:49
categories: cpp
---

We have a `std::function` that needs to be called after two different asyncronous tasks are finished. We want to run both tasks at the same time and we do not know which one will finish first.

<!-- more -->

Here's the code more or less:

```cpp
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
    /* ... */
}

int main()
{
    std::cout << "started" << std::endl;
    startTwoTasks([](){
        std::cout << "finished" << std::endl;
    });
    return 0;
}
```

When I was first confronted with this problem in C++, I remembered that is actually a very similar to the one solved by [the various async javascript libraries](https://github.com/caolan/async#parallel) that can be used in [node.js](http://nodejs.org/), where everything is asyncronous and can easily end up in the dreaded callback hell.

The initial implementation looks like this.

```cpp
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
```

The idea is we create a shared pointer to a counter with the total amount of steps and then when each step is done, we decrease the counter. When the counter reaches zero, all the steps will have finished and we can call the callback. Additionally in this example the counter is atomic, this way the steps could run in different threads.

This can be done in a nicer way writing a wrapper class.

```cpp
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
```


Now the code looks much cleaner and its easier to add additional tasks.

```cpp
void startTwoTasks(const Callback& callback)
{
	ParallelCallback parallel(callback);
	startTaskOne(parallel);
	startTaskTwo(parallel);
    parallel.check();
}
```

The advantage of using this class is that by overwriting the operator that converts back to the original callback type, we can count the amount of tasks to be done. The downside this introduces is that we could actually end up counting up and down multiple times and this would call the end callback multiple times. To solve this problem we need to add an additional atomic boolean and a `check` method. Also, instead of preincrementing the counter like in the first implementation, we postincrement it, leaving and additional step for the check call at the end.

If we wanted to do it in a similar way to javascript, we could additionally wrap everything in a variadic template.

```cpp
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
```

We could then reduce the code to one line and it can be used with as many tasks as needed.

```cpp
void startTwoTasks(const Callback& callback)
{
    ParallelCallback::run(callback, startTaskOne, startTaskTwo);
}
```

The working code for these three examples can be found [here](https://github.com/miguelibero/miguel.ibero.me/tree/master/code/cpp11_parallel_callback).
