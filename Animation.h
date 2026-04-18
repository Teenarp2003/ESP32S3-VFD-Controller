#ifndef ANIMATION_H
#define ANIMATION_H

#include <Arduino.h>
#include "config.h"
#include "StateManager.h"

class Animation {
public:
  Animation(class DisplayManager* display);
  
  void matrixCurtain(const char* newLine1, const char* newLine2, 
                     const char* oldLine1, const char* oldLine2, bool centered = true);
  void matrixCurtainExact(const char* newLine1, const char* newLine2, 
                          const char* oldLine1, const char* oldLine2);
  
  void setSpeed(int ms);
  void setCurtainWidth(int width);
  bool isAnimating();
  
private:
  class DisplayManager* display;
  int curtainWidth;
  int settleSpeed;
  const char* matrixChars;
  
  void playCenteredAnimation(const char* newLine1, const char* newLine2,
                             const char* oldLine1, const char* oldLine2);
  void playExactAnimation(const char* newLine1, const char* newLine2,
                         const char* oldLine1, const char* oldLine2);
};

#endif // ANIMATION_H
