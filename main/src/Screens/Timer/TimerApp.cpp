#include "TimerApp.h"
#include "Theme/theme.h"
#include "Devices/Input.h"
#include "Screens/MainMenu/MainMenu.h"
#include "Services/ChirpSystem.h"
#include "Services/SleepMan.h"
#include "Settings/Settings.h"
#include "Filepaths.hpp"
#include "Util/Services.h"
#include "Util/stdafx.h"
#include <cstdio>
#include <cstring>

// Colours. The background image comes from the active theme (same one the main menu uses),
// these are only for the text. Tweak them here once you've picked a palette.
static lv_color_t colAccent(){ return lv_color_make(255, 101, 0); }   // title + edit highlight (theme 1 orange)
static lv_color_t colReady(){ return lv_color_white(); }
static lv_color_t colRunning(){ return lv_color_make(60, 255, 100); }
static lv_color_t colPaused(){ return lv_color_make(255, 200, 60); }
static lv_color_t colAlarm(){ return lv_color_make(255, 60, 60); }
static lv_color_t colDim(){ return lv_color_make(190, 190, 190); }

static constexpr uint8_t DigitMax[4] = { 9, 9, 5, 9 }; // M M : S S  (seconds tens max 5)

TimerApp::TimerApp() : queue(12){
	buildUI();
}

TimerApp::~TimerApp(){
	stopAlarm();
}

void TimerApp::buildUI(){
	bg = lv_obj_create(*this);
	lv_obj_add_flag(bg, LV_OBJ_FLAG_FLOATING);
	lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_size(bg, 128, 128);
	lv_obj_set_pos(bg, 0, 0);
	lv_obj_set_style_bg_color(bg, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(bg, 0, 0);
	lv_obj_set_style_radius(bg, 0, 0);

	// Use the same background as the main menu for the current theme
	auto settings = (Settings*) Services.get(Service::Settings);
	if(settings != nullptr){
		const auto theme = settings->get().themeData.theme;
		lv_obj_set_style_bg_img_src(bg, THEMED_FILE(Menu, Background, theme), 0);
	}

	titleLabel = lv_label_create(bg);
	lv_obj_set_style_text_font(titleLabel, &devin, 0);
	lv_obj_set_style_text_color(titleLabel, colAccent(), 0);
	lv_label_set_text(titleLabel, "TIMER");
	lv_obj_align(titleLabel, LV_ALIGN_TOP_MID, 0, 18);

	// M M : S S as separate labels so a single digit can be highlighted
	row = lv_obj_create(bg);
	lv_obj_remove_style_all(row);
	lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_column(row, 0, 0);
	lv_obj_align(row, LV_ALIGN_CENTER, 0, -6);

	for(int i = 0; i < 4; i++){
		digitLabels[i] = lv_label_create(row);
		lv_obj_set_style_text_font(digitLabels[i], &clockfont, 0);
		lv_obj_set_style_radius(digitLabels[i], 2, 0);
		lv_obj_set_style_pad_hor(digitLabels[i], 1, 0);

		// colon goes between the minutes and seconds digits
		if(i == 1){
			colonLabel = lv_label_create(row);
			lv_obj_set_style_text_font(colonLabel, &clockfont, 0);
			lv_label_set_text(colonLabel, ":");
		}
	}

	statusLabel = lv_label_create(bg);
	lv_obj_set_style_text_font(statusLabel, &devin, 0);
	lv_obj_set_style_text_color(statusLabel, colDim(), 0);
	lv_obj_align(statusLabel, LV_ALIGN_CENTER, 0, 20);

	hintLabel = lv_label_create(bg);
	lv_obj_set_style_text_font(hintLabel, &devin, 0);
	lv_obj_set_style_text_color(hintLabel, colDim(), 0);
	lv_obj_set_style_text_align(hintLabel, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_align(hintLabel, LV_ALIGN_BOTTOM_MID, 0, -4);

	updateTimeDisplay();
	updateUIState();
}

void TimerApp::onStart(){
	auto sleep = (SleepMan*) Services.get(Service::Sleep);
	if(sleep != nullptr){
		sleep->enAutoSleep(false);
		// A quick Alt tap puts the watch to sleep whenever "alt lock" is on (SleepMan::handleInput),
		// which is what made Back switch the screen off. Alt is Back inside the timer, so turn it off here.
		sleep->enAltLock(false);
	}
	Events::listen(Facility::Input, &queue);
}

void TimerApp::onStop(){
	stopAlarm();
	Events::unlisten(&queue);
	auto sleep = (SleepMan*) Services.get(Service::Sleep);
	if(sleep != nullptr){
		sleep->enAutoSleep(true);
	}
}

void TimerApp::loop(){
	Event evt{};
	while(queue.get(evt, 0)){
		// transition() can tear this screen down right away. Work out beforehand whether this
		// press leaves the app, and if so touch nothing of ours afterwards (queue, state, ...).
		bool leaving = false;

		if(evt.facility == Facility::Input){
			auto data = (Input::Data*) evt.data;
			if(data->action == Input::Data::Press){
				leaving = (state == State::Ready && data->btn == Input::Alt && !exiting);
				handleButton(data->btn);
			}
		}
		free(evt.data);

		if(leaving) return;
	}

	uint64_t now = millis();

	if(state == State::Running){
		if(now - lastTickMillis >= 1000){
			uint32_t elapsed = (uint32_t)((now - lastTickMillis) / 1000);
			lastTickMillis += (uint64_t) elapsed * 1000;

			if(remainingSeconds > elapsed){
				remainingSeconds -= elapsed;
				updateTimeDisplay();
			}else{
				remainingSeconds = 0;
				state = State::Alarm;
				triggerAlarm();
				updateTimeDisplay();
				updateUIState();
			}
		}
	}

	if(state == State::Alarm){
		if(now - lastAlarmBeep >= 800){
			lastAlarmBeep = now;
			auto audio = (ChirpSystem*) Services.get(Service::Audio);
			if(audio != nullptr){
				audio->play({ { 1200, 1200, 120 }, { 0, 0, 60 }, { 1200, 1200, 120 } });
			}
		}
	}
}

/*
 * Controls
 *   Ready:   SEL start | UP/DN edit time | ALT back to menu
 *   Edit:    UP/DN change digit | SEL next digit (after the last one: done) | ALT done
 *   Running: SEL pause | ALT stop and reset
 *   Paused:  SEL resume | ALT reset
 *   Alarm:   any button stops it
 */
void TimerApp::handleButton(uint8_t btn){
	if(exiting) return;

	switch(state){
		case State::Alarm:
			stopAlarm();
			resetToReady();
			break;

		case State::Ready:
			if(btn == Input::Select){
				startTimer();
			}else if(btn == Input::Up || btn == Input::Down){
				cursor = 0;
				setState(State::Edit);
			}else if(btn == Input::Alt){
				exitToMenu();
			}
			break;

		case State::Edit:
			if(btn == Input::Up){
				adjustDigit(+1);
			}else if(btn == Input::Down){
				adjustDigit(-1);
			}else if(btn == Input::Select){
				if(cursor < 3){
					cursor++;
					updateTimeDisplay();
				}else{
					setState(State::Ready);
				}
			}else if(btn == Input::Alt){
				setState(State::Ready);
			}
			break;

		case State::Running:
			if(btn == Input::Select){
				setState(State::Paused);
			}else if(btn == Input::Alt){
				resetToReady();
			}
			break;

		case State::Paused:
			if(btn == Input::Select){
				lastTickMillis = millis();
				setState(State::Running);
			}else if(btn == Input::Alt){
				resetToReady();
			}
			break;
	}
}

void TimerApp::setState(State newState){
	state = newState;
	updateTimeDisplay();
	updateUIState();
}

void TimerApp::adjustDigit(int delta){
	int v = (int) digits[cursor] + delta;
	const int top = DigitMax[cursor];
	if(v > top) v = 0;
	if(v < 0) v = top;
	digits[cursor] = (uint8_t) v;
	updateTimeDisplay();
}

uint32_t TimerApp::setSeconds() const{
	const uint32_t mins = digits[0] * 10 + digits[1];
	const uint32_t secs = digits[2] * 10 + digits[3];
	return mins * 60 + secs;
}

void TimerApp::startTimer(){
	const uint32_t total = setSeconds();
	if(total == 0){
		lv_label_set_text(statusLabel, "SET A TIME");
		lv_obj_align(statusLabel, LV_ALIGN_CENTER, 0, 20);
		return;
	}
	remainingSeconds = total;
	lastTickMillis = millis();
	setState(State::Running);
}

void TimerApp::resetToReady(){
	remainingSeconds = setSeconds();
	setState(State::Ready);
}

void TimerApp::exitToMenu(){
	// Only ever request this once; further input is ignored from now on
	exiting = true;
	transition([](){ return std::make_unique<MainMenu>(); });
}

static void setDigitText(lv_obj_t* label, uint8_t value){
	char buf[2] = { (char) ('0' + value), 0 };
	lv_label_set_text(label, buf);
}

void TimerApp::updateTimeDisplay(){
	uint8_t d[4];
	if(state == State::Ready || state == State::Edit){
		memcpy(d, digits, sizeof(d));
	}else{
		const uint32_t mins = remainingSeconds / 60;
		const uint32_t secs = remainingSeconds % 60;
		d[0] = (uint8_t) ((mins / 10) % 10);
		d[1] = (uint8_t) (mins % 10);
		d[2] = (uint8_t) (secs / 10);
		d[3] = (uint8_t) (secs % 10);
	}

	lv_color_t textColor = colReady();
	if(state == State::Running) textColor = colRunning();
	else if(state == State::Paused) textColor = colPaused();
	else if(state == State::Alarm) textColor = colAlarm();

	for(int i = 0; i < 4; i++){
		setDigitText(digitLabels[i], d[i]);

		if(state == State::Edit && cursor == i){
			// highlighted digit: inverted block
			lv_obj_set_style_bg_color(digitLabels[i], colAccent(), 0);
			lv_obj_set_style_bg_opa(digitLabels[i], LV_OPA_COVER, 0);
			lv_obj_set_style_text_color(digitLabels[i], lv_color_black(), 0);
		}else{
			lv_obj_set_style_bg_opa(digitLabels[i], LV_OPA_TRANSP, 0);
			lv_obj_set_style_text_color(digitLabels[i], textColor, 0);
		}
	}
	lv_obj_set_style_text_color(colonLabel, textColor, 0);
}

void TimerApp::updateUIState(){
	switch(state){
		case State::Alarm:
			lv_obj_set_style_text_color(statusLabel, colAlarm(), 0);
			lv_label_set_text(statusLabel, "TIME'S UP!");
			lv_label_set_text(hintLabel, "Press ANY key");
			break;
		case State::Running:
			lv_obj_set_style_text_color(statusLabel, colRunning(), 0);
			lv_label_set_text(statusLabel, "RUNNING");
			lv_label_set_text(hintLabel, "SEL: Pause\nALT: Reset");
			break;
		case State::Paused:
			lv_obj_set_style_text_color(statusLabel, colPaused(), 0);
			lv_label_set_text(statusLabel, "PAUSED");
			lv_label_set_text(hintLabel, "SEL: Resume\nALT: Reset");
			break;
		case State::Edit:
			lv_obj_set_style_text_color(statusLabel, colAccent(), 0);
			lv_label_set_text(statusLabel, "SET TIME");
			lv_label_set_text(hintLabel, "SEL: Next\nALT: Done");
			break;
		case State::Ready:
		default:
			lv_obj_set_style_text_color(statusLabel, colDim(), 0);
			lv_label_set_text(statusLabel, "READY");
			lv_label_set_text(hintLabel, "SEL: Start\nALT: Back");
			break;
	}
	lv_obj_align(statusLabel, LV_ALIGN_CENTER, 0, 20);
	lv_obj_align(hintLabel, LV_ALIGN_BOTTOM_MID, 0, -4);
}

void TimerApp::triggerAlarm(){
	lastAlarmBeep = millis();
	auto audio = (ChirpSystem*) Services.get(Service::Audio);
	if(audio != nullptr){
		audio->play({ { 1200, 1200, 200 }, { 0, 0, 80 }, { 1200, 1200, 200 } });
	}
}

void TimerApp::stopAlarm(){
	auto audio = (ChirpSystem*) Services.get(Service::Audio);
	if(audio != nullptr){
		audio->stop();
	}
}
