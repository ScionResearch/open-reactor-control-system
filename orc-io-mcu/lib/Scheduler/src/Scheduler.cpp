#include "Scheduler.h"

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
    return _running? (_startTime + _duration - millis()) : 0;
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