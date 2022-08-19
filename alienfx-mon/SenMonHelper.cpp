#include "SenMonHelper.h"
#include <Pdh.h>
#include <PdhMsg.h>
#include "common.h"

#pragma comment(lib, "pdh.lib")

// debug print
#ifdef _DEBUG
#define DebugPrint(_x_) OutputDebugString(_x_);
#else
#define DebugPrint(_x_)
#endif

DWORD WINAPI CEventProc(LPVOID);
DWORD WINAPI CBiosProc(LPVOID);

extern ConfigMon* conf;
extern AlienFan_SDK::Control* acpi;

SenMonHelper::SenMonHelper()
{
	stopEvents = CreateEvent(NULL, true, false, NULL);

	ModifyMon();
	StartMon();
}

SenMonHelper::~SenMonHelper()
{
	StopMon();
	CloseHandle(stopEvents);
	if (acpi)
		delete acpi;
}

void SenMonHelper::StartMon()
{
	if (!eHandle) {
		ResetEvent(stopEvents);
		eHandle = CreateThread(NULL, 0, CEventProc, this, 0, NULL);
	}
}

void SenMonHelper::StopMon()
{
	if (eHandle) {
		SetEvent(stopEvents);
		WaitForSingleObject(eHandle, 1000);
		CloseHandle(eHandle);
		eHandle = 0;
	}
}

void SenMonHelper::ModifyMon()
{
	if (conf->bSensors) {
		acpi = new AlienFan_SDK::Control();
		if (!(conf->bSensors = acpi->Probe())) {
			delete acpi;
			acpi = NULL;
		}
	}
	else {
		if (acpi) {
			delete acpi;
			acpi = NULL;
		}
	}
}

void AddUpdateSensor(ConfigMon* conf, int grp, byte type, DWORD id, long val, string name) {
	SENSOR* sen;
	if (val > 10000 || val < NO_SEN_VALUE) return;
	if (sen = conf->FindSensor(grp, type, id)) {
		sen->cur = val;
		sen->min = sen->min == NO_SEN_VALUE ? val : min(sen->min, val);
		sen->max = max(sen->max, val);
	}
	else {
		// add sensor
		sen = new SENSOR({ grp, type, id, name, val, val, val, NO_SEN_VALUE });
		conf->active_sensors.push_back(*sen);
		conf->needFullUpdate = true;
	}
	if (sen->alarm && sen->oldCur && sen->oldCur != NO_SEN_VALUE) {
		// Check alarm
		if ((sen->direction ? (sen->cur < (int)sen->ap) + (sen->oldCur >= (int)sen->ap) == 2 :
			(sen->cur > (int)sen->ap) + (sen->oldCur <= (int)sen->ap) == 2)) {
			// Set alarm
			ShowNotification(&conf->niData, "Alarm triggered!", "Sensor \"" + sen->name +
				"\" " + (sen->direction ? "lower " : "higher ") + to_string(sen->ap) + " (Current: " + to_string(sen->cur) + ")!", true);
		}
	}
}

PDH_FMT_COUNTERVALUE_ITEM* counterValues = new PDH_FMT_COUNTERVALUE_ITEM[1];
DWORD counterSize = sizeof(PDH_FMT_COUNTERVALUE_ITEM);

int GetValuesArray(HCOUNTER counter) {
	PDH_STATUS pdhStatus;
	DWORD count;
	while ((pdhStatus = PdhGetFormattedCounterArray(counter, PDH_FMT_LONG /*| PDH_FMT_NOSCALE*/, &counterSize, &count, counterValues)) == PDH_MORE_DATA) {
		delete[] counterValues;
		counterValues = new PDH_FMT_COUNTERVALUE_ITEM[(counterSize / sizeof(PDH_FMT_COUNTERVALUE_ITEM)) + 1];
	}

	if (pdhStatus != ERROR_SUCCESS) {
		return 0;
	}
	return count;
}

DWORD WINAPI CEventProc(LPVOID param)
{
	SenMonHelper* src = (SenMonHelper*)param;

	LPCTSTR COUNTER_PATH_CPU = "\\Processor Information(_Total)\\% Processor Time",
		COUNTER_PATH_NET = "\\Network Interface(*)\\Bytes Total/sec",
		COUNTER_PATH_GPU = "\\GPU Engine(*)\\Utilization Percentage",
		COUNTER_PATH_HOT = "\\Thermal Zone Information(*)\\Temperature",
		COUNTER_PATH_HOT2 = "\\EsifDeviceInformation(*)\\Temperature",
		COUNTER_PATH_PWR = "\\EsifDeviceInformation(*)\\RAPL Power",
		COUNTER_PATH_HDD = "\\PhysicalDisk(_Total)\\% Idle Time";

	HQUERY hQuery = NULL;
	HCOUNTER hCPUCounter, hHDDCounter, hNETCounter, hGPUCounter, hTempCounter, hTempCounter2, hPwrCounter;

	MEMORYSTATUSEX memStat{ sizeof(MEMORYSTATUSEX) };

	SYSTEM_POWER_STATUS state;

	DWORD cType = 0, valCount = 0;

	// Set data source...
	PdhSetDefaultRealTimeDataSource(DATA_SOURCE_WBEM);

	if (PdhOpenQuery(NULL, 0, &hQuery) != ERROR_SUCCESS)
	{
		goto cleanup;
	}

	PdhAddCounter(hQuery, COUNTER_PATH_CPU, 0, &hCPUCounter);
	PdhAddCounter(hQuery, COUNTER_PATH_HDD, 0, &hHDDCounter);
	PdhAddCounter(hQuery, COUNTER_PATH_NET, 0, &hNETCounter);
	PdhAddCounter(hQuery, COUNTER_PATH_GPU, 0, &hGPUCounter);
	PdhAddCounter(hQuery, COUNTER_PATH_HOT, 0, &hTempCounter);
	PdhAddCounter(hQuery, COUNTER_PATH_HOT2, 0, &hTempCounter2);
	PdhAddCounter(hQuery, COUNTER_PATH_PWR, 0, &hPwrCounter);

	PDH_FMT_COUNTERVALUE cCPUVal, cHDDVal;

	// Bugfix for incorrect value order
	PdhCollectQueryData(hQuery);

	while (WaitForSingleObject(src->stopEvents, conf->refreshDelay) == WAIT_TIMEOUT) {
		// get indicators...
		PdhCollectQueryData(hQuery);

		if (conf->wSensors) { // group 0
			if (PdhGetFormattedCounterValue(hCPUCounter, PDH_FMT_LONG, &cType, &cCPUVal) == ERROR_SUCCESS) // CPU, code 0
				AddUpdateSensor(conf, 0, 0, 0, cCPUVal.longValue, "CPU load");

			GlobalMemoryStatusEx(&memStat); // RAM, code 1
			AddUpdateSensor(conf, 0, 1, 0, memStat.dwMemoryLoad, "Memory usage");

			if (PdhGetFormattedCounterValue(hHDDCounter, PDH_FMT_LONG, &cType, &cHDDVal) == ERROR_SUCCESS) // HDD, code 2
				AddUpdateSensor(conf, 0, 2, 0, 100 - cHDDVal.longValue, "HDD load");

			GetSystemPowerStatus(&state); // Battery, code 3
			AddUpdateSensor(conf, 0, 3, 0, state.BatteryLifePercent, "Battery");

			valCount = GetValuesArray(hGPUCounter); // GPU, code 4
			long valLast = 0;
			for (unsigned i = 0; i < valCount && counterValues[i].szName != NULL; i++) {
				if ((/*counterValues[i].FmtValue.CStatus == PDH_CSTATUS_NEW_DATA ||*/
					counterValues[i].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA))
					valLast = max(valLast, counterValues[i].FmtValue.longValue);
			}
			AddUpdateSensor(conf, 0, 4, 0, valLast, "GPU load");

			valCount = GetValuesArray(hTempCounter); // Temps, code 5
			for (unsigned i = 0; i < valCount; i++) {
				if (/*counterValues[i].FmtValue.CStatus == PDH_CSTATUS_NEW_DATA ||*/
					counterValues[i].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA)
					AddUpdateSensor(conf, 0, 5, i, counterValues[i].FmtValue.longValue - 273, counterValues[i].szName);
			}
		}

		if (conf->eSensors) { // group 1
			// ESIF temperatures and power
			valCount = GetValuesArray(hTempCounter2); // Esif temps, code 0
			// Added other tempset ...
			for (unsigned i = 0; i < valCount; i++) {
				if ((/*counterValues[i].FmtValue.CStatus == PDH_CSTATUS_NEW_DATA ||*/
					counterValues[i].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA) && counterValues[i].FmtValue.longValue)
					AddUpdateSensor(conf, 1, 0, i, counterValues[i].FmtValue.longValue, (string)"Temp " + to_string(i));
			}
			valCount = GetValuesArray(hPwrCounter); // Esif powers, code 1
			for (unsigned i = 0; i < valCount; i++) {
				if (/*counterValues[i].FmtValue.CStatus == PDH_CSTATUS_NEW_DATA ||*/
					counterValues[i].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA)
					AddUpdateSensor(conf, 1, 1, i, counterValues[i].FmtValue.longValue/2, (string)"Power " + to_string(i)/*counterValues[i].szName*/);
			}
		}

		if (conf->bSensors && acpi) { // group 2
			// Fan data and BIOS temperatures
			int val;
			for (int i = 0; i < acpi->HowManySensors(); i++) { // BIOS temps, code 0
				if (val = acpi->GetTempValue(i))
					AddUpdateSensor(conf, 2, 0, i, val, acpi->sensors[i].name);
			}

			for (int i = 0; i < acpi->HowManyFans(); i++) { // BIOS fans, code 1-3
				AddUpdateSensor(conf, 2, 1, i, acpi->GetFanRPM(i), (string)"Fan " + to_string(i+1) + " RPM");
				AddUpdateSensor(conf, 2, 2, i, acpi->GetFanPercent(i), (string)"Fan " + to_string(i+1) + " percent");
				AddUpdateSensor(conf, 2, 3, i, acpi->GetFanBoost(i, true), (string)"Fan " + to_string(i + 1) + " boost");
			}
		}
	}

cleanup:

	if (hQuery)
		PdhCloseQuery(hQuery);

	return 0;
}