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
            unsigned long start = micros();
            if (_callback) _callback();
            unsigned long elapsed = micros() - start;
            _updateStats(elapsed);
        }
    }

    void pause() { _paused = true; }
    void resume() { _paused = false; }
    bool isPaused() const { return _paused; }
    bool isHighPriority() const { return _highPriority; }
    bool operator==(const ScheduledTask& other) const {
        return _callback == other._callback && _interval == other._interval;
    }

    unsigned long getLastExecTime() const { return _lastExecTime; }
    unsigned long getMinExecTime() const { return _minExecTime; }
    unsigned long getMaxExecTime() const { return _maxExecTime; }
    float getAverageExecTime() const { return _execCount ? ((float)_totalExecTime / _execCount) : 0; }

private:
    void _updateStats(unsigned long duration) {
        _lastExecTime = duration;
        if (duration < _minExecTime || _execCount == 0) _minExecTime = duration;
        if (duration > _maxExecTime) _maxExecTime = duration;
        _totalExecTime += duration;
        ++_execCount;
    }

    TaskCallback _callback;
    unsigned long _interval;
    bool _repeat;
    bool _paused;
    bool _highPriority;
    NoBlockDelay _timer;

    // Execution timing stats
    unsigned long _lastExecTime = 0;
    unsigned long _minExecTime = 0;
    unsigned long _maxExecTime = 0;
    unsigned long _totalExecTime = 0;
    unsigned long _execCount = 0;
};

class TaskScheduler {
public:
    ScheduledTask* addTask(TaskCallback callback, unsigned long interval, bool repeat = true, bool highPriority = false) {
        ScheduledTask* task = new ScheduledTask(callback, interval, repeat, highPriority);
        _tasks.push_back(task);
        return task;
    }
    void removeTask(ScheduledTask* task) {
        _tasks.erase(std::remove(_tasks.begin(), _tasks.end(), task), _tasks.end());
        delete task;
    }


    void update() {
        for (auto* task : _tasks) {
            if (task->isHighPriority()) task->update();
        }
        for (auto* task : _tasks) {
            if (!task->isHighPriority()) task->update();
        }
    }

    ~TaskScheduler() {
        for (auto* task : _tasks) {
            delete task;
        }
        _tasks.clear();
    }

private:
    std::vector<ScheduledTask*> _tasks;
};
