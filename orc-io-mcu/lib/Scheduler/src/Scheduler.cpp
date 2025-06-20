#include "Scheduler.h"

// No-block delay class
NoBlockDelay::NoBlockDelay() {
    _startTime = 0;
    _duration = 0;
    _continuous = false;
    _running = false;
}

void NoBlockDelay::setMode(bool continuous) {
    _continuous = continuous;
}

void NoBlockDelay::start(unsigned long duration) {
    if (duration == 0) return;
    _startTime = millis();
    _duration = duration;
    _running = true;
}

void NoBlockDelay::stop() {
    _startTime = 0;
    _duration = 0;
    _running = false;
}

bool NoBlockDelay::isRunning() {
    return _running;
}

unsigned long NoBlockDelay::getRemainingTime() {
    return _running ? (_duration - (millis() - _startTime)) : 0;
}

unsigned long NoBlockDelay::getDuration() {
    return _duration;
}

bool NoBlockDelay::complete() {
    if (!_running || (millis() - _startTime) < _duration) return false;
    if (_continuous) start(_duration);
    else stop();
    return true;
}

// Scheduled task class
ScheduledTask::ScheduledTask(TaskCallback callback, unsigned long interval, bool repeat, bool highPriority)
    : _callback(callback),
      _interval(interval),
      _repeat(repeat),
      _paused(false),
      _highPriority(highPriority),
      _timer(),                 // _timer comes before stats variables
      _lastExecTime(0),
      _minExecTime(ULONG_MAX),
      _maxExecTime(0),
      _totalExecTime(0),
      _execCount(0)
{
    _timer.setMode(_repeat);
    if (_interval > 0) _timer.start(_interval);
}



void ScheduledTask::update() {
    if (!_paused && _timer.complete()) {
        unsigned long start = micros();
        if (_callback) _callback();
        unsigned long elapsed = micros() - start;
        _updateStats(elapsed);
    }
}

void ScheduledTask::pause() { _paused = true; }

void ScheduledTask::resume() { _paused = false; }

bool ScheduledTask::isPaused() const { return _paused; }

bool ScheduledTask::isHighPriority() const { return _highPriority; }

void ScheduledTask::setInterval(unsigned long interval) {
    _interval = interval;
    _timer.start(_interval);
}

unsigned long ScheduledTask::getInterval() const { return _interval; }

unsigned long ScheduledTask::getLastExecTime() const { return _lastExecTime; }

unsigned long ScheduledTask::getMinExecTime() const { return _minExecTime; }

unsigned long ScheduledTask::getMaxExecTime() const { return _maxExecTime; }

float ScheduledTask::getAverageExecTime() const { return _execCount ? ((float)_totalExecTime / _execCount) : 0; }

void ScheduledTask::resetStats() {
    _lastExecTime = 0;
    _minExecTime = ULONG_MAX;
    _maxExecTime = 0;
    _totalExecTime = 0;
    _execCount = 0;
}

void ScheduledTask::_updateStats(unsigned long duration) {
    _lastExecTime = duration;
    if (duration < _minExecTime || _execCount == 0) _minExecTime = duration;
    if (duration > _maxExecTime) _maxExecTime = duration;
    _totalExecTime += duration;
    ++_execCount;
}

// Task scheduler class
TaskScheduler::~TaskScheduler() {
    for (auto* task : _tasks) {
        delete task;
    }
    _tasks.clear();
}

ScheduledTask* TaskScheduler::addTask(TaskCallback callback, unsigned long interval, bool repeat, bool highPriority) {
    ScheduledTask* task = new ScheduledTask(callback, interval, repeat, highPriority);
    _tasks.push_back(task);
    return task;
}

void TaskScheduler::removeTask(ScheduledTask* task) {
    _tasks.erase(std::remove(_tasks.begin(), _tasks.end(), task), _tasks.end());
    delete task;
}

void TaskScheduler::update() {
    for (auto* task : _tasks) {
        if (task->isHighPriority()) task->update();
    }
    for (auto* task : _tasks) {
        if (!task->isHighPriority()) task->update();
    }
}