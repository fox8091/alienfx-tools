#include "alienfx-gui.h"
#include "EventHandler.h"
#include "MonHelper.h"
#include <Psapi.h>

// debug print
#ifdef _DEBUG
#define DebugPrint(_x_) OutputDebugString(string(_x_).c_str());
#else
#define DebugPrint(_x_)
#endif

extern AlienFan_SDK::Control* acpi;
extern EventHandler* eve;
extern FXHelper* fxhl;
extern MonHelper* mon;
extern ConfigFan* fan_conf;

extern void SetTrayTip();

void CEventProc(LPVOID);
VOID CALLBACK CForegroundProc(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
VOID CALLBACK CCreateProc(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
LRESULT CALLBACK KeyProc(int nCode, WPARAM wParam, LPARAM lParam);

EventHandler::EventHandler()
{
	eve = this;
	aProcesses = new DWORD[maxProcess];
	ChangePowerState();
	ChangeEffectMode();
}

EventHandler::~EventHandler()
{
	StopProfiles();
	StopEffects();
	delete[] aProcesses;
}

void EventHandler::ChangePowerState()
{
	SYSTEM_POWER_STATUS state;
	GetSystemPowerStatus(&state);
	if ((byte)conf->statePower != state.ACLineStatus) {
		conf->statePower = state.ACLineStatus;
		DebugPrint("Power state changed!\n");
		fxhl->SetState();
		if (cEvent)
			CheckProfileWindow();
	}
}

void EventHandler::SwitchActiveProfile(profile* newID)
{
	if (keyboardSwitchActive) return;
	if (!newID) newID = conf->FindDefaultProfile();
	if (newID != conf->activeProfile) {
		// reset effects
		fxhl->UpdateGlobalEffect(NULL, true);
		modifyProfile.lock();
		conf->activeProfile = newID;
		fan_conf->lastProf = newID->flags & PROF_FANS ? &newID->fansets : &fan_conf->prof;
		modifyProfile.unlock();

		if (acpi)
			mon->SetCurrentGmode(fan_conf->lastProf->gmode);

		fxhl->SetState();
		ChangeEffectMode();

		DebugPrint("Profile switched to " + to_string(newID->id) + " (" + newID->name + ")\n");
	}
#ifdef _DEBUG
	else
		DebugPrint("Same profile \"" + newID->name + "\", skipping switch.\n");
#endif
}

void EventHandler::ChangeEffectMode() {
	StartEffects();
	SetTrayTip();
}

void EventHandler::StopEffects() {
	if (sysmon) {	// Events
		delete sysmon; sysmon = NULL;
	}
	if (capt) {		// Ambient
		delete capt; capt = NULL;
	}
	if (audio) {	// Haptics
		delete audio; audio = NULL;
	}
	if (grid) {		// Grid
		delete grid; grid = NULL;
	}
	fxhl->Refresh();
}

void EventHandler::StartEffects() {
	if (conf->stateEffects && conf->stateOn) {
		bool haveMon = false, haveAmb = false, haveHap = false, haveGrid = false;
		for (auto it = conf->activeProfile->lightsets.begin(); it != conf->activeProfile->lightsets.end(); it++) {
			if (it->events.size()) haveMon = true;
			if (it->ambients.size()) haveAmb = true;
			if (it->haptics.size()) haveHap = true;
			if (it->effect.trigger) haveGrid = true;
		}
		if (haveMon && !sysmon)
			sysmon = new SysMonHelper(true);
		if (!haveMon && sysmon) {	// System monitoring
			delete sysmon; sysmon = NULL;
		}
		if (haveAmb && !capt)
			capt = new CaptureHelper(true);
		if (!haveAmb && capt) {		// Ambient
				delete capt; capt = NULL;
			}
		if (haveHap && !audio)
			audio = new WSAudioIn();
		if (!haveHap && audio) {	// Haptics
				delete audio; audio = NULL;
			}
		if (haveGrid)
			if (!grid)
				grid = new GridHelper();
			else
				grid->RestartWatch();
		else
			if (grid) {		// Grid
				delete grid; grid = NULL;
			}
		fxhl->Refresh();
	}
	else
		StopEffects();
}

void EventHandler::ScanTaskList() {
	DWORD cbNeeded;
	char* szProcessName = new char[32768];

	profile* newp = NULL, *finalP = NULL;

	if (EnumProcesses(aProcesses, maxProcess * sizeof(DWORD), &cbNeeded)) {
		while (cbNeeded / sizeof(DWORD) == maxProcess) {
			maxProcess = maxProcess << 1;
			delete[] aProcesses;
			aProcesses = new DWORD[maxProcess];
			EnumProcesses(aProcesses, maxProcess * sizeof(DWORD), &cbNeeded);
		}

		for (UINT i = 0; i < cbNeeded / sizeof(DWORD); i++) {
			if (aProcesses[i]) {
				HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, aProcesses[i]);
				if (hProcess && GetProcessImageFileName(hProcess, szProcessName, 32767)) {
					PathStripPath(szProcessName);
					CloseHandle(hProcess);
					// is it related to profile?
					if (newp = conf->FindProfileByApp(string(szProcessName))) {
						finalP = newp;
						if (conf->IsPriorityProfile(newp))
							break;
					}
				}
			}
			//else {
			//	DebugPrint("Zero process " + to_string(i) + "\n");
			//}
		}
	}
#ifdef _DEBUG
	if (finalP) {
		DebugPrint("TaskScan: new profile is " + finalP->name + "\n");
	}
	else
		DebugPrint("TaskScan: no profile\n");
#endif // _DEBUG
	delete[] szProcessName;
	SwitchActiveProfile(finalP);
}

void EventHandler::CheckProfileWindow() {

	char* szProcessName;
	DWORD prcId;

	GetWindowThreadProcessId(GetForegroundWindow(), &prcId);
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, prcId);

	if (hProcess) {
		szProcessName = new char[32768];
		GetProcessImageFileName(hProcess, szProcessName, 32767);
		CloseHandle(hProcess);
		PathStripPath(szProcessName);

		DebugPrint(string("Foreground switched to ") + szProcessName + "\n");

		if (!conf->noDesktop || (szProcessName != string("ShellExperienceHost.exe")
			&& szProcessName != string("explorer.exe")
			&& szProcessName != string("SearchApp.exe")
#ifdef _DEBUG
			&& szProcessName != string("devenv.exe")
#endif
			)) {

			profile* newProf;

			if (newProf = conf->FindProfileByApp(szProcessName, true)) {
				if (conf->IsPriorityProfile(newProf) || !conf->IsPriorityProfile(conf->activeProfile))
					SwitchActiveProfile(newProf);
			}
			else
				ScanTaskList();
		}
#ifdef _DEBUG
		else {
			DebugPrint("Forbidden app, switch blocked!\n");
		}
#endif // _DEBUG
		delete[] szProcessName;
	}
}

void EventHandler::StartProfiles()
{
	if (conf->enableProfSwitch && !cEvent) {
		DebugPrint("Profile hooks starting.\n");

		hEvent = SetWinEventHook(EVENT_SYSTEM_FOREGROUND,
			EVENT_SYSTEM_FOREGROUND, NULL,
			CForegroundProc, 0, 0,
			WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

		cEvent = SetWinEventHook(EVENT_OBJECT_CREATE,
			EVENT_OBJECT_DESTROY, NULL,
			CCreateProc, 0, 0,
			WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

#ifndef _DEBUG
		kEvent = SetWindowsHookExW(WH_KEYBOARD_LL, KeyProc, NULL, 0);
#endif

		// Need to switch if already running....
		CheckProfileWindow();
	}
}

void EventHandler::StopProfiles()
{
	if (cEvent) {
		DebugPrint("Profile hooks stop.\n");
		UnhookWinEvent(hEvent);
		UnhookWinEvent(cEvent);
		UnhookWindowsHookEx(kEvent);
		cEvent = NULL;
		keyboardSwitchActive = false;
	}
}

// Callback and event processing hooks
// Create - Check process ID, switch if found and no foreground active.
// Foreground - Check process ID, switch if found, clear foreground if not.
// Close - Check process list, switch if found and no foreground active.

static VOID CALLBACK CCreateProc(HWINEVENTHOOK hWinEventHook, DWORD dwEvent, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime) {

	char* szProcessName;
	DWORD prcId;

	GetWindowThreadProcessId(hwnd, &prcId);

	HANDLE hProcess;

	if (idObject == OBJID_WINDOW && !GetParent(hwnd) &&
		(hProcess = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, prcId))) {
		szProcessName = new char[32768];
		GetProcessImageFileName(hProcess, szProcessName, 32767);
		PathStripPath(szProcessName);
		//DebugPrint(("C/D: " + to_string(dwEvent) + " - " + szProcessName + "\n").c_str());
		if (conf->FindProfileByApp(szProcessName)) {
			if (dwEvent == EVENT_OBJECT_DESTROY) {
				DebugPrint("C/D: Delay activated.\n");
				// Wait for termination
				WaitForSingleObject(hProcess, 1000);
				DebugPrint("C/D: Delay done.\n");
			}
			//eve->ScanTaskList();
			eve->CheckProfileWindow();
		}
		delete[] szProcessName;
		CloseHandle(hProcess);
	}
}

static VOID CALLBACK CForegroundProc(HWINEVENTHOOK hWinEventHook, DWORD dwEvent, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime) {
	eve->CheckProfileWindow();
}

LRESULT CALLBACK KeyProc(int nCode, WPARAM wParam, LPARAM lParam) {

	LRESULT res = CallNextHookEx(NULL, nCode, wParam, lParam);

	switch (wParam) {
	case WM_KEYDOWN: case WM_SYSKEYDOWN:
		if (!eve->keyboardSwitchActive) {
			for (auto pos = conf->profiles.begin(); pos != conf->profiles.end(); pos++)
				if (((LPKBDLLHOOKSTRUCT)lParam)->vkCode == ((*pos)->triggerkey & 0xff) && conf->SamePower((*pos)->triggerFlags)) {
					eve->SwitchActiveProfile(*pos);
					eve->keyboardSwitchActive = true;
					break;
				}
		}
		break;
	case WM_KEYUP: case WM_SYSKEYUP:
		if (eve->keyboardSwitchActive && ((LPKBDLLHOOKSTRUCT)lParam)->vkCode == (conf->activeProfile->triggerkey & 0xff)) {
			eve->keyboardSwitchActive = false;
			eve->CheckProfileWindow();
		}
		break;
	}

	return res;
}
