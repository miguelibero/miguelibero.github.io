---
layout: post
title: "Implementing Signals in C++11"
date: 2014-05-04 17:42
categories: cpp
---

The *observer pattern* is used a lot in videogames when multiple game subsystems need to be notified of an event. The [original pattern](http://en.wikipedia.org/wiki/Observer_pattern) involves defining an interface that each observer will implement, but a much neater way to do it is using [signals](http://en.wikipedia.org/wiki/Signals_and_slots). In this article we discuss the evolution of this pattern in [SocialPoint](http://www.socialpoint.es)'s mobile games.

<!-- more -->

A typical example of an observable game event would be a change in the game score. Implementing this in C++ is easy.

```cpp
class IGameScoreObserver
{
public:
    virtual IGameScoreObserver(){};
    virtual void notifyGameScoreChanged(unsigned score) = 0;
};

class GameScore
{
private:
    std::vector<IGameScoreObserver*> _observers;
    unsigned _value;
public:
    GameScore(unsigned value=0) : _value(value)
    {
    }

    void setValue(unsigned value)
    {
        _value = value;
        for(auto observer : _observers)
        {
           observer->notifyGameScoreChanged(value);
        }
    }

    void addObserver(IGameScoreObserver& observer)
    {
        _observers.push_back(&observer);
    }

    void removeObserver(IGameScoreObserver& observer)
    {
        _observers.erase(std::remove(_observers.begin(), _observers.end(), &observer), _observers.end());
    }
};
```

This implementation is simple and would work, but it has two main problems:

* there is some boilerplate code that has to be written for each observer (could be improved writing an abstract subject class using templates)
* could lead to multiple inheritance in classes that implement `IGameScoreObserver` which can lead to the dreaded [diamond of death](http://en.wikipedia.org/wiki/Multiple_inheritance#The_diamond_problem)

Using signals solves these two problems by hiding the observer container inside a [function object](http://en.wikipedia.org/wiki/Function_object). In our case we first started using [boost::signal](http://www.boost.org/doc/libs/1_55_0/doc/html/signals.html) since in our first mobile games there still was no C++11 support and we were already using some of the [boost libraries](http://www.boost.org/).

Our first implementation using `boost::signal` looked something like this:

```cpp
#include <boost/signal.hpp>

class Signals
{
private:
    Signals(){};
public:
    static boost::signal<void(unsigned value)> gameScoreChanged;
};

class GameScore
{
public:

    GameScore(unsigned value=0) : _value(value)
    {
    }

    void setValue(unsigned value)
    {
        _value = value;
        Signals::gameScoreChanged(value);
    }
};
```

As you can see, much less boilerplate code and no needed interface for the game score observer. Connecting and observer is done by using `boost::function`.

```cpp
#include <boost/function.hpp>
ScoreView view;
boost::signal<void(unsigned value)>::connection connGameScoreChanged = Signals::gameScoreChanged.connect(boost::function(&ScoreView::updateScore, view, _1));
```

The `boost::signal` returns a connection object that can be used to remove the observer. There is also a  `boost::scoped_connection` that will automatically disconnect when the object is destroyed. Disconnecting the signal is important to prevent the crash that will happen when calling the `boost::function` on an object that is already destroyed.

We got this working and it seemed ok, but introduced new problems due to the way we were using it:

* `boost::signal` can return a non-void value, this value is generated from the return values of the connected functions using an additional signal template argument called a [combiner](http://www.boost.org/doc/libs/1_55_0/doc/html/signals/s06.html#idp204727928). This functionality is confusing and does not make sense in the context of the original observer pattern.
* `boost::signal` objects are all public and lumped together in a giant header. This creates artificial dependencies as well as increases the compile time.
* the implementation of the observer pattern using `boost::signal` is shown to the outside

For our newer games we were to trying to solve this problems and also trying to move to C++11 removing the boost dependency. The first thing we did was implement our own signal class.

```cpp
template<class... F>
class SignalConnection;

template<class... F>
class ScopedSignalConnection;

template<class... F>
class SignalConnectionItem
{
public:
    typedef std::function<void(F...)> Callback;
private:
    Callback _callback;
    bool _connected;

public:
    SignalConnectionItem(const Callback& cb, bool connected=true) :
    _callback(cb), _connected(connected)
    {
    }

    void operator()(F... args)
    {
        if(_connected && _callback)
        {
            _callback(args...);
        }
    }

    bool connected() const
    {
        return _connected;
    }

    void disconnect()
    {
        _connected = false;
    }
};

template<class... F>
class Signal
{
public:
    typedef std::function<void(F...)> Callback;
    typedef SignalConnection<F...> Connection;
    typedef ScopedSignalConnection<F...> ScopedConnection;

private:
    typedef SignalConnectionItem<F...> ConnectionItem;
    typedef std::list<std::shared_ptr<ConnectionItem>> ConnectionList;

    ConnectionList _list;
    unsigned _recurseCount;

    void clearDisconnected()
    {
        _list.erase(std::remove_if(_list.begin(), _list.end(), [](std::shared_ptr<ConnectionItem>& item){
            return !item->connected();
        }), _list.end());
    }

public:

    Signal() :
    _recurseCount(0)
    {
    }

    ~Signal()
    {
        for(auto& item : _list)
        {
            item->disconnect();
        }
    }

    void operator()(F... args)
    {
        std::list<std::shared_ptr<ConnectionItem>> list;
        for(auto& item : _list)
        {
            if(item->connected())
            {
                list.push_back(item);
            }
        }
        _recurseCount++;
        for(auto& item : list)
        {
            (*item)(args...);
        }
        _recurseCount--;
        if(_recurseCount == 0)
        {
            clearDisconnected();
        }
    };

    Connection connect(const Callback& callback)
    {
        auto item = std::make_shared<ConnectionItem>(callback, true);
        _list.push_back(item);
        return Connection(*this, item);
    }

    bool disconnect(const Connection& connection)
    {
        bool found = false;
        for(auto& item : _list)
        {
            if(connection.hasItem(*item) && item->connected())
            {
                found = true;
                item->disconnect();
            }
        }
        if(found)
        {
            clearDisconnected();
        }
        return found;
    }

    void disconnectAll()
    {
        for(auto& item : _list)
        {
            item->disconnect();
        }
        clearDisconnected();
    }

    friend class Connecion;
};

template<class... F>
class SignalConnection
{
private:
    typedef SignalConnectionItem<F...> Item;

    Signal<F...>* _signal;
    std::shared_ptr<Item> _item;

public:
    SignalConnection()
    : _signal(nullptr)
    {
    }

    SignalConnection(Signal<F...>& signal, const std::shared_ptr<Item>& item)
    : _signal(&signal), _item(item)
    {
    }

    void operator=(const SignalConnection& other)
    {
        _signal = other._signal;
        _item = other._item;
    }

    virtual ~SignalConnection()
    {
    }

    bool hasItem(const Item& item) const
    {
        return _item.get() == &item;
    }

    bool connected() const
    {
        return _item->connected;
    }

    bool disconnect()
    {
        if(_signal && _item && _item->connected())
        {
            return _signal->disconnect(*this);
        }
        return false;
    }
};

template<class... F>
class ScopedSignalConnection : public SignalConnection<F...>
{
public:

    ScopedSignalConnection()
    {
    }

    ScopedSignalConnection(Signal<F...>* signal, void* callback)
    : SignalConnection<F...>(signal, callback)
    {
    }

    ScopedSignalConnection(const SignalConnection<F...>& other)
    : SignalConnection<F...>(other)
    {
    }

    ~ScopedSignalConnection()
    {
        disconnect();
    }

    ScopedSignalConnection & operator=(const SignalConnection<F...>& connection)
    {
        disconnect();
        SignalConnection<F...>::operator=(connection);
        return *this;
    }
};
```

This C++11 implementation works exactly the same way as `boost::signal` and uses a [`std::shared_ptr`](http://en.cppreference.com/w/cpp/memory/shared_ptr) to share a `SignalConnectionItem`. This shared pointer is used by the connection and the signal to mark the connection as disconnected but is not public. The signal template does not allow you to return values other than void, which was a problem of `boost::signal`, and we also implemented a `ScopedConnection` class for a simpler [RAII](http://en.wikipedia.org/wiki/Resource_Acquisition_Is_Initialization) idiom.

When using the new signal we changed the implementation to hide it entirely from the observers.

```cpp
class GameScore
{
private:
    typedef Signal<unsigned> ChangedSignal;
    ChangedSignal _changedEvent;

public:
    typedef ChangedSignal::Callback ChangedCallback;
    typedef ChangedSignal::Connection ChangedConnection;
    typedef ChangedSignal::ScopedConnection ChangedScopedConnection;

    GameScore(unsigned value=0) : _value(value)
    {
    }

    void setValue(unsigned value)
    {
        _value = value;
        _changedEvent(value);
    }

    ChangedConnection addObserver(const ChangedCallback& callback)
    {
        return _changedEvent.connect(callback);
    }
};
```

This way the observer pattern implementation using the signal is not shown to the outside. We also add each signal as a property to the object that is going to generate the event, removing the giant signal list header.

Now listening to this event is much cleaner.

```cpp
class ScoreView
{
private:
    GameScore::ScopedConnection _scoreChangedConnection;

public:
    GameView(GameScore& score) :
    _scoreChangedConnection(score.addObserver(std::bind(&ScoreView::updateScore, this, std::placeholders::_1)))
    {
    }

    ~GameView()
    {
        // no need to disconnect when using ScopedConnection
    }

    void updateScore(unsigned score)
    {
        // update the score view
    }
};
```
