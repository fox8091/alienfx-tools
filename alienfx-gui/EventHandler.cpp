
#include <pdh.h>
#include <pdhmsg.h>
#pragma comment(lib, "pdh.lib")

#include "EventHandler.h"
#include "../AlienFX-SDK/AlienFX_SDK/AlienFX_SDK.h"
#include <Psapi.h>

DWORD WINAPI CEventProc(LPVOID);
DWORD WINAPI CProfileProc(LPVOID);

HANDLE dwHandle = 0, dwProfile = 0, stopEvents = 0, stopProfile = 0;
DWORD dwThreadID;

void EventHandler::ChangePowerState()
{
	SYSTEM_POWER_STATUS state;
	GetSystemPowerStatus(&state);
    bool sameState = true;
    if (state.ACLineStatus) {
        // AC line
        switch (state.BatteryFlag) {
        case 8: // charging
            sameState = fxh->SetMode(MODE_CHARGE);
            break;
        default:
            sameState = fxh->SetMode(MODE_AC);
            break;
        }
    }
    else {
        // Battery - check BatteryFlag for details
        switch (state.BatteryFlag) {
        case 1: // ok
            sameState = fxh->SetMode(MODE_BAT);
            break;
        case 2: case 4: // low/critical
            sameState = fxh->SetMode(MODE_LOW);
            break;
        }
    }
    if (!sameState) fxh->RefreshState();
}

void EventHandler::ChangeScreenState(DWORD state)
{
    if (conf->offWithScreen) {
        conf->stateOn = conf->lightsOn && state;
        fxh->Refresh(true);
    }
}

profile* EventHandler::FindProfile(int id) {
    profile* prof = NULL;
    for (int i = 0; i < conf->profiles.size(); i++)
        if (conf->profiles[i].id == id) {
            prof = &conf->profiles[i];
            break;
        }
    return prof;
}

void EventHandler::SwitchActiveProfile(int newID)
{
    if (newID != conf->activeProfile) {
        profile* newP = FindProfile(newID),
            * oldP = FindProfile(conf->activeProfile);
        if (newP != NULL) {
            StopEvents();
            if (oldP != NULL)
                oldP->lightsets = conf->active_set;
            conf->active_set = newP->lightsets;
            conf->activeProfile = newID;
            conf->monState = newP->flags & 0x2 ? 0 : conf->enableMon;
            conf->stateDimmed = newP->flags & 0x4;
            StartEvents();
    #ifdef _DEBUG
            char buff[2048];
            sprintf_s(buff, 2047, "Profile switched to #%d (\"%s\")\n", newP->id, newP->name.c_str());
            OutputDebugString(buff);
    #endif
        }
    }
}

void EventHandler::StartEvents()
{
    if (!dwHandle && conf->monState) {
        fxh->RefreshMon();
        // start thread...
#ifdef _DEBUG
        OutputDebugString("Event thread start.\n");
#endif
        stopEvents = CreateEvent(NULL, true, false, NULL);
        dwHandle = CreateThread( NULL, 0, CEventProc, this, 0, &dwThreadID);
    }
}

void EventHandler::StopEvents()
{
    DWORD exitCode;
    if (dwHandle) {
#ifdef _DEBUG
        OutputDebugString("Event thread stop.\n");
#endif
        SetEvent(stopEvents);
        WaitForSingleObject(dwHandle, 1000);
        CloseHandle(dwHandle);
        CloseHandle(stopEvents);
        dwHandle = 0;
        fxh->Refresh(true);
    }
}

void EventHandler::StartProfiles()
{
    DWORD dwThreadID;
    if (dwProfile == 0 && conf->enableProf) {
#ifdef _DEBUG
        OutputDebugString("Profile thread starting.\n");
#endif
        stopProfile = CreateEvent(NULL, true, false, NULL);
        dwProfile = CreateThread( NULL, 0, CProfileProc, this, 0, &dwThreadID);
    }
}

void EventHandler::StopProfiles()
{
    DWORD exitCode;
    if (dwProfile) {
#ifdef _DEBUG
        OutputDebugString("Profile thread stop.\n");
#endif
        SetEvent(stopProfile);
        WaitForSingleObject(dwProfile, 1000);
        CloseHandle(dwProfile);
        CloseHandle(stopProfile);
        dwProfile = 0;
    }
}

void EventHandler::ToggleEvents()
{
    if (conf->monState && conf->stateOn)
        if (!dwHandle)
            StartEvents();
        else
            fxh->Refresh();
    else
        if (!conf->monState || !conf->stateOn)
            if (dwHandle)
                StopEvents();
            else
                fxh->Refresh(true);
}

EventHandler::EventHandler(ConfigHandler* config, FXHelper* fx)
{
    conf = config;
    fxh = fx;
    StartProfiles();
    StartEvents();
}

EventHandler::~EventHandler()
{
    StopProfiles();
    StopEvents();
}

DWORD CProfileProc(LPVOID param) {
    EventHandler* src = (EventHandler*)param;
    DWORD aProcesses[1024], cbNeeded, cProcesses;
    TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");
    
    unsigned int i;

    while (WaitForSingleObject(stopProfile, 50) == WAIT_TIMEOUT) {
        unsigned newp = src->conf->activeProfile;
        bool notDefault = false;

        GUITHREADINFO activeThread;
        activeThread.cbSize = sizeof(GUITHREADINFO);

        Sleep(100);
        GetGUIThreadInfo(NULL, &activeThread);

        if (activeThread.hwndActive != 0) {
            // is it related to profile?
            DWORD nameSize = sizeof(szProcessName) / sizeof(TCHAR);
            DWORD prcId = 0;
            GetWindowThreadProcessId(activeThread.hwndActive, &prcId);
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION |
                PROCESS_VM_READ,
                FALSE, prcId);
            QueryFullProcessImageName(hProcess, 0, szProcessName, &nameSize);

            for (int j = 0; j < src->conf->profiles.size(); j++)
                if (src->conf->profiles[j].triggerapp == std::string(szProcessName)) {
                    // active app is belong to profile!
                    newp = src->conf->profiles[j].id;
                    notDefault = true;
                    break;
                }
        }

        if (!notDefault && EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
        {
            HMODULE hMod;
            cProcesses = cbNeeded / sizeof(DWORD);
            for (i = 0; i < cProcesses && !notDefault; i++)
            {
                if (aProcesses[i] != 0)
                {
                    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION |
                        PROCESS_VM_READ,
                        FALSE, aProcesses[i]);
                    if (NULL != hProcess)
                    {
                        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod),
                            &cbNeeded))
                        {
                            GetModuleFileNameEx(hProcess, hMod, szProcessName, MAX_PATH);
                            // is it related to profile?
                            for (int j = 0; j < src->conf->profiles.size(); j++)
                                if (!(src->conf->profiles[j].flags & 0x8) && src->conf->profiles[j].triggerapp == std::string(szProcessName)) {
                                    // found trigger
                                    newp = src->conf->profiles[j].id;
                                    notDefault = true;
                                    break;
                                }

                        }
                    }
                }
            }
        }
        // do we need to switch?
        if (notDefault) {
            src->SwitchActiveProfile(newp);
        }
        else 
            // switch for default profile....
            src->SwitchActiveProfile(src->conf->defaultProfile);
    }
    return 0;
}

DWORD WINAPI CEventProc(LPVOID param)
{
    EventHandler* src = (EventHandler*)param;
    
    LPCTSTR COUNTER_PATH_CPU = "\\Processor(_Total)\\% Processor Time",
        COUNTER_PATH_NET = "\\Network Interface(*)\\Bytes Total/sec",
        COUNTER_PATH_GPU = "\\GPU Engine(*)\\Utilization Percentage",
        COUNTER_PATH_HOT = "\\Thermal Zone Information(*)\\Temperature",
        COUNTER_PATH_HOT2 = "\\EsifDeviceInformation(*)\\Temperature",
        COUNTER_PATH_HDD = "\\PhysicalDisk(_Total)\\% Disk Time";

    HQUERY hQuery = NULL;
    HLOG hLog = NULL;
    PDH_STATUS pdhStatus;
    DWORD dwLogType = PDH_LOG_TYPE_CSV;
    HCOUNTER hCPUCounter, hHDDCounter, hNETCounter, hGPUCounter, hTempCounter, hTempCounter2;

    MEMORYSTATUSEX memStat;
    memStat.dwLength = sizeof(MEMORYSTATUSEX);

    SYSTEM_POWER_STATUS state;

    long maxnet = 1;

    long maxgpuarray = 10, maxnetarray = 10, maxtemparray = 10;
    PDH_FMT_COUNTERVALUE_ITEM* gpuArray = new PDH_FMT_COUNTERVALUE_ITEM[maxgpuarray];
    PDH_FMT_COUNTERVALUE_ITEM* netArray = new PDH_FMT_COUNTERVALUE_ITEM[maxnetarray];
    PDH_FMT_COUNTERVALUE_ITEM* tempArray = new PDH_FMT_COUNTERVALUE_ITEM[maxtemparray];

    // Open a query object.
    pdhStatus = PdhOpenQuery(NULL, 0, &hQuery);

    if (pdhStatus != ERROR_SUCCESS)
    {
        //wprintf(L"PdhOpenQuery failed with 0x%x\n", pdhStatus);
        goto cleanup;
    }

    // Add one counter that will provide the data.
    pdhStatus = PdhAddCounter(hQuery,
        COUNTER_PATH_CPU,
        0,
        &hCPUCounter);

    pdhStatus = PdhAddCounter(hQuery,
        COUNTER_PATH_HDD,
        0,
        &hHDDCounter);

    pdhStatus = PdhAddCounter(hQuery,
        COUNTER_PATH_NET,
        0,
        &hNETCounter);

    pdhStatus = PdhAddCounter(hQuery,
        COUNTER_PATH_GPU,
        0,
        &hGPUCounter);
    pdhStatus = PdhAddCounter(hQuery,
        COUNTER_PATH_HOT,
        0,
        &hTempCounter);

    pdhStatus = PdhAddCounter(hQuery,
        COUNTER_PATH_HOT2,
        0,
        &hTempCounter2);

    while (WaitForSingleObject(stopEvents, 150) == WAIT_TIMEOUT) {
        // wait a little...
        //Sleep(150);

        // get indicators...
        PdhCollectQueryData(hQuery);
        PDH_FMT_COUNTERVALUE cCPUVal, cHDDVal;
        DWORD cType = 0, 
            netbSize = maxnetarray * sizeof(PDH_FMT_COUNTERVALUE_ITEM), netCount = 0,
            gpubSize = maxgpuarray * sizeof(PDH_FMT_COUNTERVALUE_ITEM), gpuCount = 0,
            tempbSize = maxtemparray * sizeof(PDH_FMT_COUNTERVALUE_ITEM), tempCount = 0;
        pdhStatus = PdhGetFormattedCounterValue(
            hCPUCounter,
            PDH_FMT_LONG,
            &cType,
            &cCPUVal
        );
        pdhStatus = PdhGetFormattedCounterValue(
            hHDDCounter,
            PDH_FMT_LONG,
            &cType,
            &cHDDVal
        );

        pdhStatus = PdhGetFormattedCounterArray(
            hNETCounter,
            PDH_FMT_LONG,
            &netbSize,
            &netCount,
            netArray
        );

        if (pdhStatus != ERROR_SUCCESS) {
            if (pdhStatus == PDH_MORE_DATA) {
                maxnetarray = netbSize / sizeof(PDH_FMT_COUNTERVALUE_ITEM) + 1;
                delete[] netArray;
                netArray = new PDH_FMT_COUNTERVALUE_ITEM[maxnetarray];
            }
            netCount = 0;
        }

        // Normilizing net values...
        ULONGLONG totalNet = 0;
        for (unsigned i = 0; i < netCount; i++) {
            totalNet += netArray[i].FmtValue.longValue;
        }

        if (maxnet < totalNet) maxnet = totalNet;
        //if (maxnet / 4 > totalNet) maxnet /= 2; TODO: think about decay!
        totalNet = (totalNet * 100) / maxnet;

        pdhStatus = PdhGetFormattedCounterArray(
            hGPUCounter,
            PDH_FMT_LONG,
            &gpubSize,
            &gpuCount,
            gpuArray
        );

        if (pdhStatus != ERROR_SUCCESS) {
            if (pdhStatus == PDH_MORE_DATA) {
                maxgpuarray = gpubSize / sizeof(PDH_FMT_COUNTERVALUE_ITEM) + 1;
                delete gpuArray;
                gpuArray = new PDH_FMT_COUNTERVALUE_ITEM[maxgpuarray];
            }
            gpuCount = 0;
        }

        // Getting maximum GPU load value...
        long maxGPU = 0;
        for (unsigned i = 0; i < gpuCount && gpuArray[i].szName != NULL; i++) {
            if (maxGPU < gpuArray[i].FmtValue.longValue)
                maxGPU = gpuArray[i].FmtValue.longValue;
        }

        pdhStatus = PdhGetFormattedCounterArray(
            hTempCounter,
            PDH_FMT_LONG,
            &tempbSize,
            &tempCount,
            tempArray
        );

        if (pdhStatus != ERROR_SUCCESS) {
            if (pdhStatus == PDH_MORE_DATA) {
                maxtemparray = tempbSize / sizeof(PDH_FMT_COUNTERVALUE_ITEM) + 1;
                delete tempArray;
                tempArray = new PDH_FMT_COUNTERVALUE_ITEM[maxtemparray];
            }
            tempCount = 0;
        }

        // Getting maximum temp...
        long maxTemp = 0;
        for (unsigned i = 0; i < tempCount; i++) {
            if (maxTemp + 273 < tempArray[i].FmtValue.longValue)
                maxTemp = tempArray[i].FmtValue.longValue - 273;
        }

        // Now other temp sensor block...
        if (src->conf->esif_temp) {
            tempbSize = maxtemparray * sizeof(PDH_FMT_COUNTERVALUE_ITEM); tempCount = 0;
            pdhStatus = PdhGetFormattedCounterArray(
                hTempCounter2,
                PDH_FMT_LONG,
                &tempbSize,
                &tempCount,
                tempArray
            );

            if (pdhStatus != ERROR_SUCCESS) {
                if (pdhStatus == PDH_MORE_DATA) {
                    maxtemparray = tempbSize / sizeof(PDH_FMT_COUNTERVALUE_ITEM) + 1;
                    delete tempArray;
                    tempArray = new PDH_FMT_COUNTERVALUE_ITEM[maxtemparray];
                }
                tempCount = 0;
            }

            // Added other set maximum temp...
            for (unsigned i = 0; i < tempCount; i++) {
                if (maxTemp < tempArray[i].FmtValue.longValue)
                    maxTemp = tempArray[i].FmtValue.longValue;
            }
        }

        if (maxTemp > 100) maxTemp = 100;

        GlobalMemoryStatusEx(&memStat);
        GetSystemPowerStatus(&state);

        if (state.BatteryLifePercent > 100) state.BatteryLifePercent = 100;

        src->fxh->SetCounterColor(cCPUVal.longValue, memStat.dwMemoryLoad, maxGPU, totalNet, cHDDVal.longValue, maxTemp, state.BatteryLifePercent);
        
    }

cleanup:

    if (hQuery)
        PdhCloseQuery(hQuery);

    return 0;
}