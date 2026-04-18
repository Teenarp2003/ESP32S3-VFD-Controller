#include "Animation.h"
#include "DisplayManager.h"

Animation::Animation(DisplayManager* disp) 
  : display(disp), 
    curtainWidth(DEFAULT_CURTAIN_WIDTH), 
    settleSpeed(DEFAULT_ANIMATION_SPEED),
    matrixChars(MATRIX_CHARS) {
}

void Animation::setSpeed(int ms) {
  settleSpeed = ms;
}

void Animation::setCurtainWidth(int width) {
  curtainWidth = width;
}

bool Animation::isAnimating() {
  return StateManager::animationInProgress;
}

void Animation::matrixCurtain(const char* newLine1, const char* newLine2, 
                               const char* oldLine1, const char* oldLine2, bool centered) {
  if (centered) {
    playCenteredAnimation(newLine1, newLine2, oldLine1, oldLine2);
  } else {
    playExactAnimation(newLine1, newLine2, oldLine1, oldLine2);
  }
}

void Animation::matrixCurtainExact(const char* newLine1, const char* newLine2, 
                                   const char* oldLine1, const char* oldLine2) {
  playExactAnimation(newLine1, newLine2, oldLine1, oldLine2);
}

void Animation::playCenteredAnimation(const char* newLine1, const char* newLine2,
                                      const char* oldLine1, const char* oldLine2) {
  StateManager::animationInProgress = true;
  
  int charCount = strlen(matrixChars);
  char displayLine1[21] = "";
  char displayLine2[21] = "";
  
  int len1 = strlen(newLine1);
  int len2 = strlen(newLine2);
  int startPos1 = (DISPLAY_COLS - len1) / 2;
  int startPos2 = (DISPLAY_COLS - len2) / 2;
  if (startPos1 < 0) startPos1 = 0;
  if (startPos2 < 0) startPos2 = 0;
  
  int oldLen1 = strlen(oldLine1);
  int oldLen2 = strlen(oldLine2);
  int oldStartPos1 = (DISPLAY_COLS - oldLen1) / 2;
  int oldStartPos2 = (DISPLAY_COLS - oldLen2) / 2;
  if (oldStartPos1 < 0) oldStartPos1 = 0;
  if (oldStartPos2 < 0) oldStartPos2 = 0;
  
  for (int curtainPos = -curtainWidth; curtainPos <= DISPLAY_COLS; curtainPos++) {
    for (int i = 0; i < DISPLAY_COLS; i++) {
      if (i < curtainPos && i < DISPLAY_COLS) {
        // Already revealed: show new text
        if (i >= startPos1 && i < startPos1 + len1) {
          displayLine1[i] = newLine1[i - startPos1];
        } else {
          displayLine1[i] = ' ';
        }
        if (i >= startPos2 && i < startPos2 + len2) {
          displayLine2[i] = newLine2[i - startPos2];
        } else {
          displayLine2[i] = ' ';
        }
      } else if (i >= curtainPos && i < curtainPos + curtainWidth && i < DISPLAY_COLS) {
        // Curtain: random matrix char
        displayLine1[i] = matrixChars[random(charCount)];
        displayLine2[i] = matrixChars[random(charCount)];
      } else {
        // Not yet revealed: show old text
        if (i >= oldStartPos1 && i < oldStartPos1 + oldLen1) {
          displayLine1[i] = oldLine1[i - oldStartPos1];
        } else {
          displayLine1[i] = ' ';
        }
        if (i >= oldStartPos2 && i < oldStartPos2 + oldLen2) {
          displayLine2[i] = oldLine2[i - oldStartPos2];
        } else {
          displayLine2[i] = ' ';
        }
      }
    }
    
    displayLine1[DISPLAY_COLS] = '\0';
    displayLine2[DISPLAY_COLS] = '\0';
    
    display->setCursor(0, 0);
    display->print(displayLine1);
    display->setCursor(0, 1);
    display->print(displayLine2);
    
    delay(settleSpeed);
  }
  
  StateManager::animationInProgress = false;
}

void Animation::playExactAnimation(const char* newLine1, const char* newLine2,
                                   const char* oldLine1, const char* oldLine2) {
  StateManager::animationInProgress = true;
  
  int charCount = strlen(matrixChars);
  char displayLine1[21] = "";
  char displayLine2[21] = "";
  
  for (int i = 0; i < DISPLAY_COLS; i++) {
    displayLine1[i] = ' ';
    displayLine2[i] = ' ';
  }
  displayLine1[DISPLAY_COLS] = '\0';
  displayLine2[DISPLAY_COLS] = '\0';
  
  // Don't clear - let the animation wipe from old content to new content
  
  for (int curtainPos = -curtainWidth; curtainPos <= DISPLAY_COLS; curtainPos++) {
    for (int i = 0; i < DISPLAY_COLS; i++) {
      if (i < curtainPos && i < DISPLAY_COLS) {
        // Already revealed: show new text
        displayLine1[i] = newLine1[i];
        displayLine2[i] = newLine2[i];
      } else if (i >= curtainPos && i < curtainPos + curtainWidth && i < DISPLAY_COLS) {
        // Curtain: random matrix char
        displayLine1[i] = matrixChars[random(charCount)];
        displayLine2[i] = matrixChars[random(charCount)];
      } else {
        // Not yet revealed: show old text
        displayLine1[i] = oldLine1[i];
        displayLine2[i] = oldLine2[i];
      }
    }
    
    display->setCursor(0, 0);
    display->print(displayLine1);
    display->setCursor(0, 1);
    display->print(displayLine2);
    delay(settleSpeed);
  }
  
  StateManager::animationInProgress = false;
}
