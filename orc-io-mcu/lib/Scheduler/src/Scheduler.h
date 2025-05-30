#pragma once

#include <Arduino.h>
#include <vector>
#include <algorithm>

/*
   A simple library to integrate nonblocking delays using millis().
   Can be used directly, or with the scheduler to call multiple timed tasks from loop().

   Example usage:

   TaskScheduler scheduler;

   void blinkLED() {
       digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
   }

   void setup() {
       pinMode(LED_BUILTIN, OUTPUT);
       task1 = scheduler.addTask(blinkLED, 1000, true, true);  // Blink every 1 second, high priority
   }

   void loop() {
       scheduler.update();
   }
*/

class NoBlockDelay {
public:
    NoBlockDelay();
    void setMode(bool continuous);
    void start(unsigned long duration);
    void stop();
    bool isRunning();
    unsigned long getRemainingTime();
    unsigned long getDuration();
    bool complete();

private:
    unsigned long _startTime = 0;
    unsigned long _duration = 0;
    bool _continuous = false;
    bool _running = false;
};

typedef void (*TaskCallback)();

class ScheduledTask {
public:
    ScheduledTask() : _callback(nullptr), _interval(0), _repeat(false), _paused(false), _highPriority(false) {}

    ScheduledTask(TaskCallback callback, unsigned long interval, bool repeat = true, bool highPriority = false)
        : _callback(callback), _interval(interval), _repeat(repeat), _paused(false), _highPriority(highPriority), _timer() {
        _timer.setMode(_repeat);
        if (_interval > 0) _timer.start(_interval);
    }

    void update() {
        if (!_paused && _timer.complete()) {
            if (_callback) _callback();
        }
    }

    void pause() { _paused = true; }
    void resume() { _paused = false; }
    bool isPaused() const { return _paused; }
    bool isHighPriority() const { return _highPriority; }
    bool operator==(const ScheduledTask& other) const {
        return _callback == other._callback && _interval == other._interval;
    }

private:
    TaskCallback _callback;
    unsigned long _interval;
    bool _repeat;
    bool _paused;
    bool _highPriority;
    NoBlockDelay _timer;
};

class TaskScheduler {
public:
    ScheduledTask* addTask(TaskCallback callback, unsigned long interval, bool repeat = true, bool highPriority = false) {
        _tasks.emplace_back(callback, interval, repeat, highPriority);
        return &_tasks.back();
    }

    void removeTask(ScheduledTask* task) {
        _tasks.erase(std::remove_if(_tasks.begin(), _tasks.end(), [task](const ScheduledTask& t) {
            return &t == task;
        }), _tasks.end());
    }

    void update() {
        // High priority tasks first
        for (auto& task : _tasks) {
            if (task.isHighPriority()) {
                task.update();
            }
        }
        // Then low priority
        for (auto& task : _tasks) {
            if (!task.isHighPriority()) {
                task.update();
            }
        }
    }

private:
    std::vector<ScheduledTask> _tasks;
};