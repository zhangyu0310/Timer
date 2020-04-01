//
// Created by poppinzhang on 2020/4/1.
//

#include <iostream>
#include <chrono>
#include <thread>
#include <manager_timer.h>

std::string getNowTime() {
    auto now = std::chrono::system_clock::now();
    auto c_time_t = std::chrono::system_clock::to_time_t(now);
    struct tm calendar{};
    localtime_r(&c_time_t, &calendar);
    char str[1024];
    strftime(str, 1024, "%Y-%m-%d %H:%M:%S", &calendar);
    return str;
}

std::string for_test(const std::string& from) {
    std::cout << from << ": " << getNowTime() << std::endl;
    return from + std::to_string(1219);
}

void for_test1(const std::string& from) {
    std::cout << from << ": " << getNowTime() << std::endl;
}

int main() {
    ManagerTimer mt;
    mt.init();
    mt.start();
    auto pair = mt.addJobRunAt(
            std::chrono::system_clock::now(), for_test, "At");
    // If want to use other time accuracy, use 'chrono' to get it.
    auto pair1 = mt.addJobRunAfter(5, for_test, "After");
    auto timer = mt.addJobRunEvery(1, for_test1, "Every");
    pair.second.wait();
    std::cout << "Function for_test return: " << pair.second.get() << std::endl;
    pair1.second.wait();
    std::cout << "Function for_test return: " << pair1.second.get() << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Function for_test1 handle time: " <<
        timer->getHandlingTime().time_since_epoch().count() << std::endl;
    timer->stopRepeat();
    auto timer2 = mt.addJobRepeatAtMinute(0, for_test1, "EveryMinute");
    timer->stopRepeat();
    // At hour & day is not easy to show.
    mt.stopAndJoin();
}