#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// Colors
const uint16_t COLOR_BG      = 0x0010; // near black
const uint16_t COLOR_BG_FLASH = 0x0A12;
const uint16_t COLOR_TEXT    = 0xFFE0; // pale yellow-white
const uint16_t COLOR_BTN     = 0xF800; // dark blue
const uint16_t COLOR_BTN_HI  = 0x07E0; // brighter blue
const uint16_t COLOR_BTN_TXT = 0xFFFF;

// Button geometry (adjust to your display size)
const int BTN_X = 90;
const int BTN_Y = 160;
const int BTN_W = 170;
const int BTN_H = 60;

// *** NEW: Define the physical button pin ***
const int START_BUTTON_PIN = 2; // Assuming GPIO 2 for the physical button

unsigned long lastFlash = 0;
unsigned long nextFlash = 0;
int flashState = 0;        // 0 = off, 1 = flashing
int flashIntensity = 0;
bool userTriggeredStart = false; // Renamed from userPressed
bool inSplashState = true; // State machine flag

void setup() {
  Serial.begin(115200);
  
  // Initialize the physical button pin as INPUT_PULLUP
  pinMode(START_BUTTON_PIN, INPUT_PULLUP);

  // 1. Initialize the TFT Display
  tft.init();
  tft.setRotation(1); // landscape

  // 2. Draw the initial splash screen
  drawSplash(false);
  
  // 3. Invert display only if required by hardware (use with caution)
  // tft.invertDisplay(true); 
}

// --- Helper Function to Draw Thick Lines (Replaces setLineWidth) ---
void drawThickLine(int x1, int y1, int x2, int y2, uint16_t color) {
    // Draw a line of effective thickness (e.g., 3 pixels wide)
    // This draws several slightly offset lines to simulate thickness.
    tft.drawLine(x1, y1, x2, y2, color);
    tft.drawLine(x1 + 1, y1 + 1, x2 + 1, y2 + 1, color);
    tft.drawLine(x1, y1 + 1, x2, y2, color);
}


void drawSplash(bool flash) {
  uint16_t bg = flash ? COLOR_BG_FLASH : COLOR_BG;
  tft.fillScreen(bg);

  // Title
  // Smaller font size and stylized offset
  tft.setTextSize(3); 
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(40, 70);
  tft.print("Auggie's");

  // Stylized "Game" part
  tft.setTextColor(0x07E0); // Light Blue accent for "Game"
  tft.setCursor(140, 70);
  tft.print("Game");

  // Lightning bolt graphic next to title (Using the custom function)
  // Adjust coordinates to match new text position
  // Strike 1
  drawThickLine(280, 50, 295, 75, flash ? 0xFFFF : 0xF800);
  // Strike 2
  drawThickLine(295, 75, 280, 75, flash ? 0xFFFF : 0xF800);
  // Strike 3
  drawThickLine(280, 75, 295, 100, flash ? 0xFFFF : 0xF800);

  // Start button
  drawButton(false);
}

void drawButton(bool highlighted) {
  // Smaller button geometry
  int w = 100;
  int h = 40;
  int x = 145; // Centered roughly
  int y = 180; // Positioned below title

  tft.fillRoundRect(x, y, w, h, 10, highlighted ? COLOR_BTN_HI : COLOR_BTN);
  tft.setTextColor(COLOR_BTN_TXT);
  tft.setTextSize(2);
  int txtW = tft.textWidth("START");
  tft.setCursor(x + (w - txtW) / 2, y + 12);
  tft.print("START");
}

// Check the physical button status
bool isStartButtonPressed() {
  // Button is active LOW because we used INPUT_PULLUP
  if (digitalRead(START_BUTTON_PIN) == LOW) {
    return true;
  }
  return false;
}

void loop() {
  unsigned long now = millis();

  // --- 1. Handle Splash Screen Animation (If we are still in the splash state) ---
  if (inSplashState) {
    
    // Handle Lightning Animation Timing
    if (flashState == 0) {
      if (nextFlash == 0) {
        nextFlash = now + random(1000, 4000);
      } else if (now >= nextFlash) {
        flashState = 1;
        flashIntensity = 2 + random(0, 2);
      }
    } else {
      // Flash on/off rapidly
      static unsigned long lastToggle = 0;
      if (now - lastToggle > 80) {
        lastToggle = now;
        // Redraw based on intensity parity to create the flicker
        if (flashIntensity % 2 == 1) drawSplash(true);
        else drawSplash(false);
        flashIntensity--;
      }
      if (flashIntensity <= 0) {
        flashState = 0;
        nextFlash = 0;
        drawSplash(false);
      }
    }

    // Handle Button Input
    if (isStartButtonPressed() && !userTriggeredStart) {
      userTriggeredStart = true; // Set flag immediately
      // Wait until the physical button is released to prevent repeated triggers
      while(digitalRead(START_BUTTON_PIN) == LOW) {
          delay(50); // Wait briefly in loop() until release
      }
      
      // Transition state immediately upon release
      inSplashState = false;
      startGame(); // Transition to game state
    }
  }
  
  // --- 2. Game State (If the button was pressed) ---
  if (!inSplashState) {
      // Keep the game logic running here
  }

  // Small delay to prevent the loop from running too fast for the animation logic
  delay(20); 
}

// Call this function when the user successfully presses the button
void startGame() {
  // Clear the screen and reset the state
  tft.fillScreen(TFT_BLACK); 
  
  // Reset animation variables
  flashState = 0;
  nextFlash = 0;
  inSplashState = false;
  
  Serial.println("--- SPAWN! Transitioning to Game State ---");

  // Draw a confirmation message instead of the lightning animation
  tft.setTextSize(2);
  tft.setTextColor(0x07E0);
  tft.drawString("GAME ACTIVE", 20, 100, 1); // Draw confirmation text
}
