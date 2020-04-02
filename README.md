# Timer

> A C++11 cross-platform Timer implementation on POSIX.

### Easy to use

Can easy add a one-time timing task or repeat timing task.

`addJobRunAt(expiration, cb_func, args...)`

`addJobRunAfter(duration, cb_func, args...)`

`addJobRunEvery(duration, cb_func, args...)`

`addJobRepeatAtDay(hour, minute, sec, cb_func, args...)`

`addJobRepeatAtHour(minute, sec, cb_func, args...)`

`addJobRepeatAtMinute(sec, cb_func, args...)`

### One time task can get return value.

> Just for `addJobRunAt` & `addJobRunAfter`

Example:

```
auto pair = timer->addJobRunAt(
                system_clock::now(), for_test);
pair.second.wait();
auto result = pair.second.get();
```

### Use thread pool asynchronous processing (Option)

Task can run in thread pool asynchronously.

```
ThreadPool tp;
ManagerTimer(&tp);
```

> 1. If tp == nullptr run task in timer thread.
>
> 2. ThreadPool need to supply 'enqueue' function to add task.
> 
> 3. You can use dep/ThreadPool. Thanks for Jakob Progsch.

### Usage

Read `demo.cpp` can get it.

### Attention

1. Must link library `rt` & `pthread`.
2. `ManagerTimer` must call `init()` & `start` before add timing task.

**Have Fun.**