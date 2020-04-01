//
// Created by poppinzhang on 2020/3/31.
//

#include "manager_timer.h"
#include <gtest/gtest.h>
#include "ThreadPool.h"
#include <iostream>
#include <locale>

ThreadPool* tp;
ManagerTimer* timer_m;

class FooEnvironment : public testing::Environment {
public:
    void SetUp() final {
        tp = new ThreadPool(10);
        timer_m = new ManagerTimer;
        timer_m->setThreadPool(tp);
        timer_m->setOverTime(std::chrono::hours(1));
        // timer_m = new ManagerTimer(nullptr);
    }
    void TearDown() final {
        delete tp;
    }
};

unsigned int for_test(unsigned int times, int* p) {
    for (unsigned int i = 0; i < times; ++i) {
        std::cout << i << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    *p = 100;
    return times;
}

int for_test1() {
    static int a = 0;
    std::cout << a << std::endl;
    return a++;
}

void for_test2(const std::string& from) {
    auto now = std::chrono::system_clock::now();
    auto c_time_t = std::chrono::system_clock::to_time_t(now);
    struct tm calendar{};
    localtime_r(&c_time_t, &calendar);
    char str[1024];
    strftime(str, 1024, "%Y-%m-%d %H:%M:%S", &calendar);
    std::cout << from << ": " << str << std::endl;
}

TEST (BaseFuncTest, initFunc) {
    char err[1024];
    bool ret = timer_m->init(err);
    if (!ret) {
        std::cout << err << std::endl;
    }
    ASSERT_TRUE(ret);
    ret = timer_m->start(err);
    if (!ret) {
        std::cout << err << std::endl;
    }
    ASSERT_TRUE(ret);
}

TEST (BaseFuncTest, addTimerNotRepeat) {
    int *p = new int;
    *p = 0;
    auto pair = timer_m->addJobRunAt(ManagerTimer::Clock::now(),
            for_test, 3, p);
    auto pair1 = timer_m->addJobRunAt(std::chrono::time_point_cast<
            std::chrono::seconds>(std::chrono::system_clock::now()),
            for_test, 5, p);
    auto pair2 = timer_m->addJobRunAt(std::chrono::system_clock::now(),
            for_test, 4, p);
    auto pair3 = timer_m->addJobRunAfter(6, for_test, 6, p);
    pair.second.wait();
    ASSERT_TRUE(pair.second.get() == 3);
    pair1.second.wait();
    ASSERT_TRUE(pair1.second.get() == 5);
    pair2.second.wait();
    ASSERT_TRUE(pair2.second.get() == 4);
    pair3.second.wait();
    ASSERT_TRUE(pair3.second.get() == 6);
}

TEST (BaseFuncTest, addTimerRepeat) {
    auto timer = timer_m->addJobRunEvery(std::chrono::seconds(1), for_test1);
    for (int i = 0; i < 20; ++i) {
        std::cout << timer->getHandlingTime().time_since_epoch().count() << std::endl;
        if (timer->isOverTime()) {
            std::cout << "Is over time." << std::endl;
            ASSERT_TRUE(false);
            break;
        }
        sleep(1);
    }
    timer->stopRepeat();
    sleep(1);
    auto deal_time = timer->getHandlingTime();
    for (int i = 0; i < 5; ++i) {
        if (deal_time == timer->getHandlingTime()) {
            sleep(1);
            deal_time = timer->getHandlingTime();
        } else {
            ASSERT_TRUE(false);
        }
    }
}

TEST (BaseFuncTest, addTimerAtTime) {
    auto now = std::chrono::system_clock::now();
    auto c_time_t = std::chrono::system_clock::to_time_t(now);
    struct tm calendar{};
    localtime_r(&c_time_t, &calendar);
    int hour = calendar.tm_hour + 1;
    int min = calendar.tm_min + 2;
    auto timer = timer_m->addJobRepeatAtMinute(10, for_test2, "min");
    ASSERT_TRUE(timer != nullptr);
    auto timer1 = timer_m->addJobRepeatAtHour(min, 0, for_test2, "hour");
    ASSERT_TRUE(timer1 != nullptr);
    auto timer2 = timer_m->addJobRepeatAtDay(hour, 0, 0, for_test2, "day");
    ASSERT_TRUE(timer2 != nullptr);
    int new_hour = calendar.tm_hour;
    while (new_hour != hour) {
        c_time_t = std::chrono::system_clock::to_time_t(now);
        localtime_r(&c_time_t, &calendar);
        new_hour = calendar.tm_hour;
        sleep(60);
    }
    timer_m->stop();
}

int main(int argc, char *argv[]) {
    // 分析gtest程序的命令行参数
    testing::AddGlobalTestEnvironment(new FooEnvironment);
    testing::InitGoogleTest(&argc, argv);

    // 调用RUN_ALL_TESTS()运行所有测试用例
    // main函数返回RUN_ALL_TESTS()的运行结果
    return RUN_ALL_TESTS();
}