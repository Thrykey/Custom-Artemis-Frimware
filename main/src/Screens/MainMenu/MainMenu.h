#ifndef CLOCKSTAR_FIRMWARE_MAINMENU_H
#define CLOCKSTAR_FIRMWARE_MAINMENU_H

#include <map>
#include <memory>
#include "LV_Interface/LVScreen.h"
#include "UIElements/StatusBar.h"
#include "MenuItem.h"
#include "Notifs/Phone.h"
#include "Devices/Input.h"
#include "MenuItemAlt.h"
#include "Filepaths.hpp"
#include "Settings/Settings.h"

class MainMenu : public LVScreen {
public:
	MainMenu();
	virtual ~MainMenu();

	static void resetMenuIndex();

private:
	static constexpr const char* ConnDesc[3] = {
			"Not connected.\nUse phone to pair",
			"Connected to\nAndroid device",
			"Connected to\niOS device"
	};

	// One name per menu slot. To add/remove an app: change this enum, the ItemInfos table below
	// (same order!), setupItemPaths() and onClick() in MainMenu.cpp, all using these names.
	enum Slot : uint8_t {
		SlotFind,
		SlotTimer,
		SlotLunar,
		SlotLevel,
		SlotConnection,
		SlotTheremin,
		SlotSettings,
		SlotCount
	};

	struct ItemInfo {
		const char* labelText = nullptr;
		const char* labelTextAlt = nullptr;
		const char* iconPath = nullptr;
		const char* iconAltPath = nullptr;
	};
	// Order must match enum Slot above
	ItemInfo ItemInfos[SlotCount] = {
			{ "Find my phone",    "Ringing phone...",                     File::Menu::Theme1::Find,       File::Menu::Theme1::Find },       // SlotFind
			{ "Timer",            nullptr,                                File::Menu::Theme1::Timer,      nullptr },                        // SlotTimer
			{ "Lunar lander",     nullptr,                                File::Menu::Theme1::Lunar,      nullptr },                        // SlotLunar
			{ "Level",            nullptr,                                File::Menu::Theme1::Level,      nullptr },                        // SlotLevel
			{ "Phone connection", ConnDesc[(int) Phone::PhoneType::None], File::Menu::Theme1::Connection, File::Menu::Theme1::Connection }, // SlotConnection
			{ "Theremin",         nullptr,                                File::Menu::Theme1::Theremin,   nullptr },                        // SlotTheremin
			{ "Settings",         nullptr,                                File::Menu::Theme1::Settings,   nullptr }                         // SlotSettings
	};
	static constexpr uint8_t ItemCount = SlotCount;
	void setupItemPaths(Theme theme);

	void setConnAlts();

	bool findPhoneState = false;
	uint32_t findPhoneCounter = 0;
	static constexpr uint32_t FindPhonePeriod = 1000; //1 second

	static uint8_t lastIndex;

	Phone& phone;

	StatusBar* statusBar;

	lv_obj_t* bg;
	lv_obj_t* container;
	MenuItem* items[ItemCount];
	// MenuItemAlt* phoneConnection;

	void onStart() override;
	void onStarting() override;
	void onStop() override;
	void loop() override;
	EventQueue queue;

	void onClick();

	void handlePhoneChange(Phone::Event& event);
	void handleInput(Input::Data& event);

	int lastDefocus = 0;
};


#endif //CLOCKSTAR_FIRMWARE_MAINMENU_H
