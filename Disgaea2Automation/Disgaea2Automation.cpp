// Disgaea2Automation.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

std::tuple<DWORD, HANDLE> findEmulatorProcess() {
	DWORD processIds[1024] = {};
	DWORD bytesReturned = 0;
	if (!EnumProcesses(processIds, sizeof(processIds), &bytesReturned))
		std::cout << GetLastError() << " at EnumProcesses" << std::endl;

	TCHAR processName[1024] = {};
	for (int p = 0; p < bytesReturned / sizeof(DWORD); ++p) {
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

unsigned char readByte(unsigned int addr) {
	unsigned char data = 0;
	if (!ReadProcessMemory(g_emulatorProcessHandle, (void*)addr, &data, 1, nullptr))
		std::cout << GetLastError() << " at ReadProcessMemory" << std::endl;
	return data;
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

//run at the prize selection screen
//will cycle out and into the menu until an item with rarity 8 appears
//(you must select the item yourself)
void rareHospitalPrize() {
	while (true) {
		unsigned char hospitalListSize = readByte(0x203EC668);
		if (hospitalListSize == 0) break;
		bool stop = false;
		for (int i = 0; i < hospitalListSize; ++i) {
			unsigned char rarity = readByte(0x203EC77A + i * 0x110);
			if (rarity == 8) stop = true;
		}
		if (stop) break;
		sendKey('B');
		Sleep(100);
		sendKey('A');
		Sleep(100);
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

	

//	char x;
//	std::cin >> x;
    return 0;
}

