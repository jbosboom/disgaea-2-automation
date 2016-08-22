// Disgaea2Automation.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

std::tuple<DWORD, HANDLE> findEmulatorProcess() {
	DWORD processIds[1024] = {};
	DWORD bytesReturned = 0;
	if (!EnumProcesses(processIds, sizeof(processIds), &bytesReturned))
		std::cout << GetLastError() << " at EnumProcesses" << std::endl;

	TCHAR processName[1024] = {};
	for (unsigned int p = 0; p < bytesReturned / sizeof(DWORD); ++p) {
		HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processIds[p]);
		if (hProc) {
			DWORD namelen = GetModuleFileNameEx(hProc, NULL, processName, 1024);
			if (!namelen)
				std::cout << GetLastError() << " at GetModuleFileNameEx" << std::endl;
			if (_tcsstr(processName, _T("pcsx2.exe")))
				return std::make_tuple(processIds[p], hProc);
			CloseHandle(hProc);
		}
	}
	return std::make_tuple((DWORD)0, (HANDLE)0);
}

BOOL CALLBACK FindGSdxWindow(HWND hWnd, LPARAM param) {
	DWORD emulatorProcessId = *((DWORD*)param);
	DWORD windowProcessId = 0;
	GetWindowThreadProcessId(hWnd, &windowProcessId);
	if (windowProcessId != emulatorProcessId) return TRUE;
	TCHAR caption[1024] = {};
	if (GetWindowText(hWnd, caption, 1024) && _tcsstr(caption, _T("GSdx"))) {
		*((HWND*)param) = hWnd;
		std::cout << "Found GSdx window" << std::endl;
		return FALSE;
	}
	return TRUE;
}

BOOL CALLBACK FindPanelChildWindow(HWND hWnd, LPARAM param) {
	TCHAR caption[1024] = {};
	if (GetWindowText(hWnd, caption, 1024) && _tcsstr(caption, _T("panel"))) {
		*((HWND*)param) = hWnd;
		std::cout << "Found GSdx child window" << std::endl;
		return FALSE;
	}
	return TRUE;
}

DWORD g_emulatorProcessId;
HANDLE g_emulatorProcessHandle;
HWND g_gsdxWnd;
HWND g_gsdxChildWnd;

template<typename T>
T read(unsigned int addr) {
	T data;
	if (!ReadProcessMemory(g_emulatorProcessHandle, (void*)addr, &data, sizeof(data), nullptr))
		std::cout << GetLastError() << " at ReadProcessMemory" << std::endl;
	return data;
}

template<typename T, int N>
void read(unsigned int addr, T (&data)[N]) {
	if (!ReadProcessMemory(g_emulatorProcessHandle, (void*)addr, &data, sizeof(data), nullptr))
		std::cout << GetLastError() << " at ReadProcessMemory" << std::endl;
}

void sendKey(char key) {
	PostMessage(g_gsdxChildWnd, WM_KEYDOWN, key, MapVirtualKey(key, MAPVK_VK_TO_VSC));
	Sleep(100);
	PostMessage(g_gsdxChildWnd, WM_KEYUP, key, MapVirtualKey(key, MAPVK_VK_TO_VSC));
}

void waitForGSdxFocus() {
	//std::cout << "GSDX " << g_gsdxWnd << " child " << g_gsdxChildWnd << std::endl;
	while (GetForegroundWindow() != g_gsdxWnd) {
		//std::cout << GetForegroundWindow() << std::endl;
		Sleep(500);
	}
	Sleep(1000);
}

#pragma pack(push, 1)
//http://akurasu.net/wiki/Makai_Senki_Disgaea_2/Save_Hacking
struct Specialist {
	uint16_t level, unknown0, jobId, nameId;
};

struct Stats {
	int32_t hp, sp, atk, def, Int, spd, hit, res;
};

struct Item {
	Specialist specialists[10];
	Stats stats, baseStats;
	uint16_t typeId;
	uint16_t level;
	uint16_t unknown0[11];
	uint8_t rarity;
	uint16_t unknown1;
	uint8_t population;
	uint8_t unknown2[27];
	uint8_t name[40]; //strange two-byte encoding
	uint8_t unknown3[31];
};
#pragma pack(pop)

const static unsigned int ITEM_BAG_SLOT_1 = 0x20384C18;
const static unsigned int BONUS_GAUGE_SLOT_1 = 0x20430C40;
//exp/HL reward values are stored after the item (which is all-zero in that case)
const static unsigned int BONUS_GAUGE_STRIDE = 0x118;

//run at the prize selection screen
//will cycle out and into the menu until an item with rarity 8 appears
//(you must select the item yourself)
void rareHospitalPrize() {
	while (true) {
		unsigned char hospitalListSize = read<uint8_t>(0x203EC668);
		if (hospitalListSize == 0) break;
		bool stop = false;
		for (int i = 0; i < hospitalListSize; ++i) {
			unsigned char rarity = read<uint8_t>(0x203EC77A + i * 0x110);
			if (rarity == 8) stop = true;
		}
		if (stop) break;
		sendKey('B');
		Sleep(100);
		sendKey('A');
		Sleep(100);
	}
}

void bonusGaugeRank49Item() {
	std::cout << std::hex;
	std::vector<uint16_t> desiderata = {
		//rank 39 stuff
		//0x008B, //God Hand
		0x00EF, //Excalibur
		//0x0153, //Holy Longinus
		0x01B7, //Artemis
		0x021B, //Megiddo Cannon
		0x027F, //Beam Axe
		//0x02E3, //Infernal Staff
		0x0347, //Satan's Motor
		//0x0379, //The King
		0x040F, //Infernal Armor

		0x0582, //Providence
	};
	//call with stage selected
	sendKey(VK_F1);
	Sleep(5000);

	while (true) {
		sendKey('A');
		Sleep(4000); //expects turbo mode

		for (unsigned int i = 0; i <= 9; ++i) {
			Item item = read<Item>(BONUS_GAUGE_SLOT_1 + i*BONUS_GAUGE_STRIDE);
			std::cout << item.typeId << "," << (int)item.rarity << " ";
			if (std::find(desiderata.begin(), desiderata.end(), item.typeId) != desiderata.end() && item.rarity < 8)
				return;
		}
		std::cout << std::endl;

		sendKey(VK_F3); //load
		Sleep(1000); //move the RNG seed (?)
		sendKey(VK_F1);
		Sleep(1000);
	}
}

int main() {
	std::tuple<DWORD, HANDLE> processData = findEmulatorProcess();
	g_emulatorProcessId = std::get<DWORD>(processData);
	g_emulatorProcessHandle = std::get<HANDLE>(processData);
	g_gsdxWnd = (HWND)g_emulatorProcessId;
	EnumWindows(FindGSdxWindow, (LPARAM)&g_gsdxWnd);
	EnumChildWindows(g_gsdxWnd, FindPanelChildWindow, (LPARAM)&g_gsdxChildWnd);
	waitForGSdxFocus();

	bonusGaugeRank49Item();

	char x;
	std::cin >> x;
    return 0;
}

