#include "InputManager.h"
#include <pico/multicore.h>

// Static member definitions
std::atomic<int> InputManager::newPos(0);
std::atomic<bool> InputManager::resetPos(false);
std::atomic<bool> InputManager::blipTone(false);
std::atomic<bool> InputManager::enterButtonPressed(false);
std::atomic<bool> InputManager::printButtonPressed(false);

// PIO globals for the static task
static PIO pio_instance = pio0;
static uint sm_instance = 0;

/**
 * @brief Default constructor for InputManager
 */
InputManager::InputManager()
{
}

/**
 * @brief Initializes encoder PIO, buttons, buzzer, and launches Core 1 task
 */
void InputManager::begin()
{
  // Setup PIO for encoder
  pio_add_program(pio_instance, &quadrature_encoder_program);
  quadrature_encoder_program_init(pio_instance, sm_instance, ROTARY_PIN_A, 0);

  // Buttons
  pinMode(ENTERBUTTON_PIN, INPUT_PULLUP);
  pinMode(PRINTBUTTON_PIN, INPUT_PULLUP);
  digitalWrite(ENTERBUTTON_PIN, HIGH);
  digitalWrite(PRINTBUTTON_PIN, HIGH);
  attachInterrupt(ENTERBUTTON_PIN, ButtonPressISR, FALLING);
  attachInterrupt(PRINTBUTTON_PIN, ButtonPressISR, FALLING);

  // Buzzer
  pinMode(Buzzer_PWM_PIN, OUTPUT);

  // Start Core 1
  multicore_launch_core1(core1Task);

  resetInactivityTimer();
}

/**
 * @brief Main loop update
void InputManager::update()
{
  //do nothing for now
}

/**
 * @brief Returns the current encoder position
 * @return Current encoder position value
 */
int InputManager::getEncoderPosition()
{
  return newPos.load(std::memory_order_acquire);
}

/**
 * @brief Sets the encoder position to a specific value
 * @param pos New position value
 */
void InputManager::setEncoderPosition(int pos)
{
  newPos.store(pos, std::memory_order_release);
}

/**
 * @brief Resets the encoder position to zero and triggers PIO reset
 */
void InputManager::resetEncoderPosition()
{
  newPos.store(0, std::memory_order_release);
  resetPos.store(true, std::memory_order_release);
}

/**
 * @brief Checks if encoder has moved
 * @return false 
 */
bool InputManager::hasEncoderMoved()
{
  // Not used for now
  return false;
}

/**
 * @brief Checks if Enter button (on encoder) was pressed
 * @return true if Enter button was pressed since last clear
 */
bool InputManager::isEnterPressed()
{
  return enterButtonPressed.load(std::memory_order_acquire);
}

/**
 * @brief Checks if Print button was pressed
 * @return true if Print button was pressed since last clear
 */
bool InputManager::isPrintPressed()
{
  return printButtonPressed.load(std::memory_order_acquire);
}

/**
 * @brief Clears the Enter button pressed flag
 */
void InputManager::clearEnterButton()
{
  enterButtonPressed.store(false, std::memory_order_release);
}

/**
 * @brief Clears the Print button pressed flag
 */
void InputManager::clearPrintButton()
{
  printButtonPressed.store(false, std::memory_order_release);
}

/**
 * @brief Checks if a blip tone should be played (set by encoder movement)
 * @return true if blip tone flag is set
 */
bool InputManager::shouldBlip()
{
  return blipTone.load(std::memory_order_acquire);
}

/**
 * @brief Clears the blip tone flag
 */
void InputManager::clearBlip()
{
  blipTone.store(false, std::memory_order_release);
}

/**
 * @brief Manually triggers a blip tone (sets flag for main loop to play)
 */
void InputManager::triggerBlip()
{
  blipTone.store(true, std::memory_order_release);
}

/**
 * @brief Plays a tone on the buzzer with specified frequency and duration
 * @param freq Frequency in Hz
 * @param ratio PWM duty cycle (0-255)
 * @param duration Duration in milliseconds
 */
void InputManager::sendBlipTone(uint32_t freq, int ratio, uint32_t duration)
{
  analogWriteFreq(freq);
  analogWrite(Buzzer_PWM_PIN, ratio);
  sleep_ms(duration);
  analogWrite(Buzzer_PWM_PIN, 0);
}

/**
 * @brief Resets the inactivity timer to current time
 */
void InputManager::resetInactivityTimer()
{
  lastActivityTime = millis();
}

/**
 * @brief Checks if inactivity timeout has been exceeded
 * @return true if inactive for longer than INACTIVITY_TIMEOUT_MS
 */
bool InputManager::checkInactivityTimeout()
{
  return (millis() - lastActivityTime > INACTIVITY_TIMEOUT_MS);
}

/**
 * @brief Returns the last activity timestamp
 * @return Timestamp in milliseconds
 */
unsigned long InputManager::getLastActivityTime()
{
  return lastActivityTime;
}

/**
 * @brief Sends a string as USB keyboard keystrokes with delays between characters
 * @param textToSend String to type out
 */
void InputManager::sendStringAsKeystrokes(const std::string &textToSend)
{
  delay(50);
  for (unsigned int i = 0; i < textToSend.length(); i++)
  {
    char c = textToSend[i];
    if (Keyboard.write(c) == 0)
    {
      break;
    }
    delay(50);
  }
}

/**
 * @brief Starts USB keyboard emulation
 */
void InputManager::beginKeyboard()
{
  if (!keyboardRunning)
  {
    Keyboard.begin();
    keyboardRunning = true;
  }
}

/**
 * @brief Stops USB keyboard emulation and restores Serial
 */
void InputManager::endKeyboard()
{
  if (keyboardRunning)
  {
    Keyboard.end();
    keyboardRunning = false;
  }
}

/**
 * @brief Checks if USB keyboard emulation is currently active
 * @return true if keyboard is running
 */
bool InputManager::isKeyboardRunning()
{
  return keyboardRunning;
}

/**
 * @brief ISR for button presses with debouncing
 *
 * Handles both Enter and Print buttons with 250ms debounce delay.
 */
void InputManager::ButtonPressISR()
{
  static unsigned long lastEnterPressTime = 0;
  static unsigned long lastPrintPressTime = 0;
  const unsigned long currentTime = millis();
  constexpr unsigned long debounceDelay = 250;

  if (digitalRead(ENTERBUTTON_PIN) == LOW)
  {
    if (currentTime - lastEnterPressTime > debounceDelay)
    {
      enterButtonPressed.store(true, std::memory_order_release);
      lastEnterPressTime = currentTime;
    }
  }
  if (digitalRead(PRINTBUTTON_PIN) == LOW)
  {
    if (currentTime - lastPrintPressTime > debounceDelay)
    {
      printButtonPressed.store(true, std::memory_order_release);
      lastPrintPressTime = currentTime;
    }
  }
}

/**
 * @brief Helper function to reset the PIO state machine Y register to zero
 *
 * Used when encoder position is reset to synchronize PIO hardware state.
 *
 * @param pio_instance PIO instance (pio0 or pio1)
 * @param sm_instance State machine number
 */
static void reset_pio_y_register(PIO pio_instance, uint sm_instance)
{
  pio_sm_set_enabled(pio_instance, sm_instance, false);
  pio_sm_exec(pio_instance, sm_instance, 0xa040); // mov y, 0
  pio_sm_clear_fifos(pio_instance, sm_instance);
  pio_sm_restart(pio_instance, sm_instance);
  pio_sm_set_enabled(pio_instance, sm_instance, true);
}

/**
 * @brief Core 1 task that continuously monitors encoder and updates position
 *
 * Runs on the second CPU core. Reads raw PIO encoder counts, converts them to
 * logical application steps (4 raw counts = 1 step), and triggers blip tones
 * on position changes. Handles encoder reset requests from main core.
 */
void InputManager::core1Task()
{
  int32_t last_raw_pio_for_app_step = quadrature_encoder_get_count(pio_instance, sm_instance);
  int32_t last_newPos_value_for_blip = newPos.load(std::memory_order_acquire);

  while (true)
  {
    int32_t current_raw_pio = quadrature_encoder_get_count(pio_instance, sm_instance);
    int32_t current_logical_app_pos = newPos.load(std::memory_order_acquire);

    if (resetPos.load(std::memory_order_acquire))
    {
      reset_pio_y_register(pio_instance, sm_instance);
      current_raw_pio = quadrature_encoder_get_count(pio_instance, sm_instance);
      last_raw_pio_for_app_step = current_raw_pio;
      last_newPos_value_for_blip = current_logical_app_pos;
      resetPos.store(false, std::memory_order_release);
    }

    const int32_t delta_raw = current_raw_pio - last_raw_pio_for_app_step;
    int32_t next_logical_app_pos = current_logical_app_pos;
    bool app_pos_updated_this_cycle = false;
    constexpr int32_t RAW_COUNTS_PER_APP_STEP = 4;

    if (delta_raw >= RAW_COUNTS_PER_APP_STEP)
    {
      const int num_steps = delta_raw / RAW_COUNTS_PER_APP_STEP;
      next_logical_app_pos += num_steps;
      last_raw_pio_for_app_step += num_steps * RAW_COUNTS_PER_APP_STEP;
      app_pos_updated_this_cycle = true;
    }
    else if (delta_raw <= -RAW_COUNTS_PER_APP_STEP)
    {
      const int num_steps = -delta_raw / RAW_COUNTS_PER_APP_STEP;
      next_logical_app_pos -= num_steps;
      last_raw_pio_for_app_step -= num_steps * RAW_COUNTS_PER_APP_STEP;
      app_pos_updated_this_cycle = true;
    }

    if (app_pos_updated_this_cycle)
    {
      newPos.store(next_logical_app_pos, std::memory_order_release);
      if (last_newPos_value_for_blip != next_logical_app_pos)
      {
        blipTone.store(true, std::memory_order_release);
        last_newPos_value_for_blip = next_logical_app_pos;
      }
    }
    sleep_ms(1);
  }
}
