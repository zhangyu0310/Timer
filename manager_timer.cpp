//
// Created by poppinzhang on 2020/3/26.
//

#include "manager_timer.h"

#include <csignal>

#include "ThreadPool.h"

ManagerTimer::~ManagerTimer() {
    if (running_) {
        running_ = false;
    }
    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
    timer_delete(timer_id_);
}

bool ManagerTimer::init(char *err) {
    if (init_) {
        return true;
    }
    // Register alarm call back function.
    struct sigevent evp{};
    evp.sigev_notify = SIGEV_THREAD;
    evp.sigev_value.sival_ptr = this;
    evp._sigev_un._sigev_thread._function = &alarmFunction;
    // Create system timer.
    if (timer_create(CLOCK_MONOTONIC, &evp, &timer_id_) == -1) {
        if (err != nullptr) {
            snprintf(err, 1024, "Init timer failed. Errno: %d", errno);
        }
        return false;
    }
    init_ = true;
    return true;
}

bool ManagerTimer::start(char* err) {
    if (!init_) {
        return false;
    }
    running_ = true;
    // New thread.
    try {
        loop_thread_ = std::thread(&ManagerTimer::loop, this);
    } catch (std::exception& e) {
        snprintf(err, 1024,
                "Create new thread failed. Error: %s", e.what());
        return false;
    }
    return true;
}

void ManagerTimer::stopAndJoin() {
    running_ = false;
    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
}

void ManagerTimer::alarm() {
    std::unique_lock<std::mutex> lk(mutex_);
    auto now_time = std::chrono::time_point_cast<Accuracy>(
            std::chrono::steady_clock::now());
    if (now_time > now_time_) {
        now_time_ = now_time;
    }
    lk.unlock();
    cv_.notify_one();
}

void ManagerTimer::loop() {
    while (running_) {
        std::unique_lock<std::mutex> lk(mutex_);
        // Wait for notify or 1 hour.
        cv_.wait_for(lk, Seconds(3600));
        auto timer_iter = timer_map_.begin();
        for (; timer_iter != timer_map_.end();) {
            if (now_time_ >= timer_iter->first) {
                // Check if the timer is over.
                auto& timer = timer_iter->second;
                bool over_time = ((now_time_ - timer_iter->first) > over_time_);
                timer->is_over_time_ = over_time;
                if (!over_time) {
                    if (thread_pool_ == nullptr) {
                        timer->cb_func_();
                    } else {
                        if (timer->repeat_) {
                            thread_pool_->enqueue(timer->cb_func_);
                        } else {
                            thread_pool_->enqueue(std::move(timer->cb_func_));
                        }
                    }
                    timer->handling_time_ = now_time_;
                }
                repeatFunc(timer);
                timer_iter = timer_map_.erase(timer_iter);
            } else {
                // No expiration timer. Break the loop & wait alarm.
                break;
            }
        }
        // Set new time alarm.
        if (timer_iter != timer_map_.end()) {
            setNewAlarm(timer_iter->second->expiration_);
        }
    }
}

void ManagerTimer::repeatFunc(const TimerPtr& timer) {
    if (timer->repeat_ &&
        timer->duration_ > Accuracy::zero()) {
        auto new_expiration = timer->expiration_ + timer->duration_;
        timer->expiration_ = new_expiration;
        timer_map_.insert(std::make_pair(new_expiration, timer));
    }
}

void ManagerTimer::setNewAlarm(const TimePoint& expiration) {
    // Set new time alarm.
    auto now = std::chrono::time_point_cast<Accuracy>(Clock::now());
    struct itimerspec in_value{};
    // Trigger immediately.
    if (expiration <= now) {
        in_value.it_value.tv_sec = 0;
        in_value.it_value.tv_nsec = 1;
    } else {
        auto next_interval = expiration - now;
        in_value.it_value.tv_sec = std::chrono::duration_cast<Seconds>(next_interval).count();
        in_value.it_value.tv_nsec = std::chrono::duration_cast<NanoSec>(next_interval).count();
        // The nano second can't bigger than 999,999,999.
        in_value.it_value.tv_nsec -= in_value.it_value.tv_sec * NanoSecPerSec;
    }
    timer_settime(timer_id_, 0, &in_value, nullptr);
}

void ManagerTimer::alarmFunction(union sigval val) {
    auto mt_ptr = static_cast<ManagerTimer*>(val.sival_ptr);
    mt_ptr->alarm();
}
