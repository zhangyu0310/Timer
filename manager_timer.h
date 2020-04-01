//
// Created by poppinzhang on 2020/3/26.
//

#ifndef MANAGER_TIMER_H
#define MANAGER_TIMER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <type_traits>

class Timer {
    friend class ManagerTimer;
    // Clock & Accuracy can be changed.
    using Clock = std::chrono::steady_clock;
    using Accuracy = Clock::duration;
    using TimePoint = std::chrono::time_point<Clock, Accuracy>;
    using CallBackFunc = std::function<void()>;
public:
    explicit Timer(const TimePoint& expiration) :
            repeat_(false),
            is_over_time_(false),
            expiration_(expiration),
            duration_(Accuracy::zero()),
            handling_time_(Accuracy::max()) { }
    explicit Timer(const Accuracy& duration) :
            repeat_(true),
            is_over_time_(false),
            expiration_(std::chrono::time_point_cast<Accuracy>(
                    Clock::now() + duration)),
            duration_(duration),
            handling_time_(Accuracy::max()) { }
    Timer(const TimePoint& expiration, const Accuracy& duration) :
            repeat_(true),
            is_over_time_(false),
            expiration_(expiration),
            duration_(duration),
            handling_time_(Accuracy::max()) { }
    void stopRepeat() {
        repeat_ = false;
    }
    bool isOverTime() const {
        return is_over_time_;
    }
    TimePoint getHandlingTime() const {
        return handling_time_;
    }

private:
    std::atomic_bool repeat_;
    std::atomic_bool is_over_time_;
    TimePoint expiration_;
    Accuracy duration_;
    TimePoint handling_time_;
    CallBackFunc cb_func_;
};

class ThreadPool;

/*class TimerError : std::runtime_error {
public:
    explicit TimerError(const std::string& msg) :
        std::runtime_error(msg) {}
};*/

class ManagerTimer {
public:
    using Clock = Timer::Clock;
    using Accuracy = Timer::Accuracy;
private:
    using TimePoint = Timer::TimePoint;
    using TimerPtr = std::shared_ptr<Timer>;
    using TimerMap = std::multimap<TimePoint, TimerPtr>;
    using Seconds = std::chrono::seconds;
    using NanoSec = std::chrono::nanoseconds;
    const long NanoSecPerSec = 1000000000;
public:
    explicit ManagerTimer(ThreadPool* thread_pool = nullptr) :
                     init_(false),
                     running_(false),
                     timer_id_(nullptr),
                     thread_pool_(thread_pool),
                     over_time_(Accuracy::max()) {
        now_time_ = std::chrono::time_point_cast<Accuracy>(Clock::now());
    }
    ManagerTimer(const ManagerTimer&) = delete;
    ManagerTimer& operator= (const ManagerTimer&) = delete;
    ~ManagerTimer();

    bool init(char* err = nullptr);
    bool start(char* err = nullptr);
    void stop() {
        running_ = false;
    }
    void stopAndJoin();
    void alarm();

    void setThreadPool(ThreadPool* tp) {
        thread_pool_ = tp;
    }
    template <typename A>
    void setOverTime(const A& duration) {
        over_time_ = std::chrono::duration_cast<Accuracy>(duration);
    }
    // Run at time point.
    template <typename Func, typename... Args>
    auto addJobRunAt(const ManagerTimer::TimePoint& expiration,
            Func&& cb_func, Args&&... args)
            -> std::pair<TimerPtr, std::future<typename std::result_of<Func(Args...)>::type>>;
    template <typename C, typename A, typename Func, typename... Args>
    auto addJobRunAt(const std::chrono::time_point<C, A>& expiration,
            Func&& cb_func, Args&&... args)
            ->std::pair<TimerPtr, std::future<typename std::result_of<Func(Args...)>::type>>;
    // Run After time duration.
    template <typename Func, typename... Args>
    auto addJobRunAfter(const ManagerTimer::Accuracy& duration,
            Func&& cb_func, Args&&... args)
            -> std::pair<TimerPtr, std::future<typename std::result_of<Func(Args...)>::type>>;
    template <typename Func, typename... Args>
    auto addJobRunAfter(unsigned long int seconds,
            Func&& cb_func, Args&&... args)
            -> std::pair<TimerPtr, std::future<typename std::result_of<Func(Args...)>::type>>;
    template <typename Rep, typename Per, typename Func, typename... Args>
    auto addJobRunAfter(const std::chrono::duration<Rep, Per>& duration,
            Func&& cb_func, Args&&... args)
            -> std::pair<TimerPtr, std::future<typename std::result_of<Func(Args...)>::type>>;
    // Run Every time duration.
    template <typename Func, typename... Args>
    TimerPtr addJobRunEvery(const ManagerTimer::Accuracy& duration,
            Func&& cb_func, Args&&... args);
    template <typename Func, typename... Args>
    TimerPtr addJobRunEvery(unsigned long int seconds,
            Func&& cb_func, Args&&... args);
    template<typename Rep, typename Per, typename Func, typename... Args>
    TimerPtr addJobRunEvery(const std::chrono::duration<Rep, Per>& duration,
            Func&& cb_func, Args&&... args);
    // Run at every time point of day/hour/minute.
    template <typename Func, typename... Args>
    TimerPtr addJobRepeatAtDay(uint hour, uint min, uint sec,
            Func&& cb_func, Args&&... args);
    template <typename Func, typename... Args>
    TimerPtr addJobRepeatAtHour(uint min, uint sec,
            Func&& cb_func, Args&&... args);
    template <typename Func, typename... Args>
    TimerPtr addJobRepeatAtMinute(uint sec, Func&& cb_func, Args&&... args);

private:
    void loop();
    void repeatFunc(const TimerPtr& timer);
    void setNewAlarm(const TimePoint& expiration);
    template <typename Func, typename... Args>
    TimerPtr addJobRepeatAt(const std::chrono::system_clock::time_point& alarm_time,
            const std::chrono::system_clock::duration& duration,
            Func&& cb_func, Args&&... args);

    static void alarmFunction(union sigval val);

    std::atomic_bool init_;
    std::atomic_bool running_;
    timer_t timer_id_;
    TimerMap timer_map_;
    std::thread loop_thread_;
    ThreadPool* thread_pool_;

    std::mutex mutex_;
    std::condition_variable cv_;
    TimePoint now_time_; // Project by mutex_ & cv_.

    Accuracy over_time_;
};

template <typename Func, typename... Args>
auto ManagerTimer::addJobRunAt(
        const ManagerTimer::TimePoint& expiration,
        Func&& cb_func, Args&&... args)
        -> std::pair<ManagerTimer::TimerPtr,
        std::future<typename std::result_of<Func(Args...)>::type>> {
    std::shared_ptr<Timer> timer(new Timer(expiration));
    using return_type = typename std::result_of<Func(Args...)>::type;
    auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<Func>(cb_func), std::forward<Args>(args)...)
    );
    timer->cb_func_ = ([task](){ (*task)(); });
    timer_map_.emplace(std::make_pair(timer->expiration_, timer));
    setNewAlarm(timer->expiration_);
    return std::make_pair(timer, task->get_future());
}

template <typename C, typename A, typename Func, typename... Args>
auto ManagerTimer::addJobRunAt(
        const std::chrono::time_point<C, A>& expiration,
        Func&& cb_func, Args&&... args)
        -> std::pair<ManagerTimer::TimerPtr,
        std::future<typename std::result_of<Func(Args...)>::type>> {
    // If clock is not same. Use 'Clock::now + (expiration - C::now)'
    // If accuracy is not same, need cast.
    auto exp = std::chrono::time_point_cast<ManagerTimer::Accuracy>(
            Clock::now() + std::chrono::duration_cast<Clock::duration>(
                    expiration - std::chrono::time_point_cast<A>(C::now())));
    return addJobRunAt(exp, cb_func, args...);
}

template<typename Func, typename... Args>
auto ManagerTimer::addJobRunAfter(
        const ManagerTimer::Accuracy& duration,
        Func&& cb_func, Args&&... args)
        -> std::pair<ManagerTimer::TimerPtr,
        std::future<typename std::result_of<Func(Args...)>::type>> {
    auto expiration = std::chrono::time_point_cast<Accuracy>(Clock::now() + duration);
    return addJobRunAt(expiration, cb_func, args...);
}

template <typename Func, typename... Args>
auto ManagerTimer::addJobRunAfter(
        unsigned long int seconds,
        Func&& cb_func, Args&&... args)
        -> std::pair<ManagerTimer::TimerPtr,
        std::future<typename std::result_of<Func(Args...)>::type>> {
    return addJobRunAfter(ManagerTimer::Seconds(seconds), cb_func, args...);
}

template<typename Rep, typename Per, typename Func, typename... Args>
auto ManagerTimer::addJobRunAfter(
        const std::chrono::duration<Rep, Per>& duration,
        Func&& cb_func, Args&&... args)
        ->std::pair<ManagerTimer::TimerPtr,
        std::future<typename std::result_of<Func(Args...)>::type>> {
    return addJobRunAfter(std::chrono::duration_cast<Accuracy>(duration), cb_func, args...);
}

template <typename Func, typename... Args>
ManagerTimer::TimerPtr ManagerTimer::addJobRunEvery(
        const ManagerTimer::Accuracy& duration,
        Func&& cb_func, Args&&... args) {
    std::shared_ptr<Timer> timer(new Timer(duration));
    timer->cb_func_ = std::bind(std::forward<Func>(cb_func), std::forward<Args>(args)...);
    timer_map_.emplace(std::make_pair(timer->expiration_, timer));
    setNewAlarm(timer->expiration_);
    return timer;
}

template <typename Func, typename... Args>
ManagerTimer::TimerPtr ManagerTimer::addJobRunEvery(
        unsigned long int seconds,
        Func&& cb_func, Args&&... args) {
    return addJobRunEvery(ManagerTimer::Seconds(seconds), cb_func, args...);
}

template<typename Rep, typename Per, typename Func, typename... Args>
ManagerTimer::TimerPtr ManagerTimer::addJobRunEvery(
        const std::chrono::duration<Rep, Per>& duration,
        Func&& cb_func, Args&&... args) {
    auto dur = std::chrono::duration_cast<Accuracy>(duration);
    return addJobRunEvery(dur, cb_func, args...);
}

// This function must ensure parameter is valid.
// If not have this parameter, input -1.
// sec can't be -1.
static std::chrono::system_clock::time_point
getAlarmTime(int hour, int min, int sec) {
    auto c_time_t = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
    struct tm calendar{};
    if (nullptr == localtime_r(&c_time_t, &calendar)) {
        return std::chrono::system_clock::time_point::min();
    }
    if (hour != -1) {
        calendar.tm_hour = hour;
    }
    if (min != -1) {
        calendar.tm_min = min;
    }
    calendar.tm_sec = sec;
    c_time_t = mktime(&calendar);
    return std::chrono::system_clock::from_time_t(c_time_t);
}

template <typename Func, typename... Args>
ManagerTimer::TimerPtr ManagerTimer::addJobRepeatAtDay(
        uint hour, uint min, uint sec,
        Func&& cb_func, Args&&... args) {
    if (hour > 23 || min > 59 || sec > 59) {
        return nullptr;
    }
    auto alarm_time = getAlarmTime(hour, min, sec);
    if (alarm_time == std::chrono::system_clock::time_point::min()) {
        return nullptr;
    }
    return addJobRepeatAt(alarm_time, std::chrono::hours(24), cb_func, args...);
}

template <typename Func, typename... Args>
ManagerTimer::TimerPtr ManagerTimer::addJobRepeatAtHour(
        uint min, uint sec, Func&& cb_func, Args&&... args) {
    if (min > 59 || sec > 59) {
        return nullptr;
    }
    auto alarm_time = getAlarmTime(-1, min, sec);
    if (alarm_time == std::chrono::system_clock::time_point::min()) {
        return nullptr;
    }
    return addJobRepeatAt(alarm_time, std::chrono::hours(1), cb_func, args...);
}

template <typename Func, typename... Args>
ManagerTimer::TimerPtr ManagerTimer::addJobRepeatAtMinute(
        uint sec, Func&& cb_func, Args&&... args) {
    if (sec > 59) {
        return nullptr;
    }
    auto alarm_time = getAlarmTime(-1, -1, sec);
    if (alarm_time == std::chrono::system_clock::time_point::min()) {
        return nullptr;
    }
    return addJobRepeatAt(alarm_time, std::chrono::minutes(1), cb_func, args...);
}

template <typename Func, typename... Args>
ManagerTimer::TimerPtr ManagerTimer::addJobRepeatAt(
        const std::chrono::system_clock::time_point& alarm_time,
        const std::chrono::system_clock::duration& duration,
        Func&& cb_func, Args&&... args) {
    using C = std::chrono::system_clock;
    using A = std::chrono::system_clock::duration;
    auto exp = std::chrono::time_point_cast<ManagerTimer::Accuracy>(
            Clock::now() + std::chrono::duration_cast<Clock::duration>(
                    alarm_time - std::chrono::time_point_cast<A>(C::now())));
    std::shared_ptr<Timer> timer(new Timer(exp, duration));
    timer->cb_func_ = std::bind(std::forward<Func>(cb_func), std::forward<Args>(args)...);
    timer_map_.emplace(std::make_pair(timer->expiration_, timer));
    setNewAlarm(timer->expiration_);
    return timer;
}

#endif //MANAGER_TIMER_H
