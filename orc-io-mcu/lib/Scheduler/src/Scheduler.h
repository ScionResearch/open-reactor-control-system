#pragma once

#include <Arduino.h>
#include <vector>
#include <algorithm>

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
    unsigned long _startTime;
    unsigned long _duration;
    bool _continuous;
    bool _running;
};

typedef void (*TaskCallback)();

class ScheduledTask {
public:
    ScheduledTask(TaskCallback callback, unsigned long interval, bool repeat = true, bool highPriority = false);

    void update();

    void pause();
    void resume();
    bool isPaused() const;
    bool isHighPriority() const;

    void setInterval(unsigned long interval);
    unsigned long getInterval() const;

    unsigned long getLastExecTime() const;
    unsigned long getMinExecTime() const;
    unsigned long getMaxExecTime() const;
    float getAverageExecTime() const;

    void resetStats();

private:
    void _updateStats(unsigned long duration);

    TaskCallback _callback;
    unsigned long _interval;
    bool _repeat;
    bool _paused;
    bool _highPriority;
    NoBlockDelay _timer;

    unsigned long _lastExecTime = 0;
    unsigned long _minExecTime = ULONG_MAX;
    unsigned long _maxExecTime = 0;
    unsigned long _totalExecTime = 0;
    unsigned long _execCount = 0;
};

class TaskScheduler {
public:
    ~TaskScheduler();

    ScheduledTask* addTask(TaskCallback callback, unsigned long interval, bool repeat = true, bool highPriority = false);
    void removeTask(ScheduledTask* task);
    void update();

private:
    std::vector<ScheduledTask*> _tasks;
};
