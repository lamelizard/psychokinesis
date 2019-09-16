#pragma once

#include <functional>

class SM {
  typedef std::function<void(int ticks)> action;

private:
  int currentTicks = 0;
  action currentAction = [](int) {}; // best of all lambdas
public:
  int getCurrentTicks() { return currentTicks; }
  void tick() { currentAction(currentTicks++); }
  void setAction(action a) {
    currentAction = a;
    currentTicks = 0;
  }
};
