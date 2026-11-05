#ifndef CLOCKSTAR_FIRMWARE_TIMERAPP_H
#define CLOCKSTAR_FIRMWARE_TIMERAPP_H

#include "LV_Interface/LVScreen.h"
#include "Util/Events.h"
#include <cstdint>

class TimerApp : public LVScreen {
public:
	TimerApp();
	~TimerApp() override;

private:
	void onStart() override;
	void onStop() override;
	void loop() override;

	enum class State : uint8_t { Ready, Edit, Running, Paused, Alarm };

	void buildUI();
	void handleButton(uint8_t btn);
	void setState(State newState);
	void adjustDigit(int delta);
	void startTimer();
	void resetToReady();
	void exitToMenu();
	void updateTimeDisplay();
	void updateUIState();
	void triggerAlarm();
	void stopAlarm();

	uint32_t setSeconds() const;

	EventQueue queue;

	lv_obj_t* bg = nullptr;
	lv_obj_t* titleLabel = nullptr;
	lv_obj_t* row = nullptr;
	lv_obj_t* digitLabels[4] = { nullptr, nullptr, nullptr, nullptr };
	lv_obj_t* colonLabel = nullptr;
	lv_obj_t* statusLabel = nullptr;
	lv_obj_t* hintLabel = nullptr;

	// Time that is set, one value per digit: M M : S S (default 05:00)
	uint8_t digits[4] = { 0, 5, 0, 0 };
	uint8_t cursor = 0; // which digit is being edited (State::Edit only)

	uint32_t remainingSeconds = 300;
	uint64_t lastTickMillis = 0;
	uint64_t lastAlarmBeep = 0;

	State state = State::Ready;
	bool exiting = false; // set once we've asked to go back to the menu, ignores further input
};

#endif //CLOCKSTAR_FIRMWARE_TIMERAPP_H
