# Alienfx-gui

`Alienfx-gui` is AWCC in 500kb. Not 500Mb!  

What it **cannot** do in compare to AWCC?
- No overclocking. Anyway, ThrottleStop or XTU do the job better.
- No audio control. Being honest, never use it in AWCC.
- No huge picture or animations, interface is as minimalistic as possible.
- No services and drivers which send you usage data in Dell, consume RAM and slow down system.

But what it can do, instead?
- Control AlienFX lights. Any way you can imagine:
  - Set the chain of effects to every light, or
  - Set the light to react any system event - CPU load, temperature, HDD access - name it.
  - Group the lights any way you wish and control group not as one light only, but as a gauge of LEDs.
  - It fast!
- Control system fans. Instead of AWCC, fan control is dynamic, so you can boost them any way you want. Or unboost.
- Can have a profile for any application you run, switching not only light sets, but, for example, dim lights or only switch if application in foreground.
- Fast, less buggy and robust LightFX support (emulation).

## Usage
Run `alienfx-gui.exe`.  
First, use "Devices and Lights" tab to configure out devices and lights settings, if you don't run `Alienfx-probe` yet.  
Use "Colour" tab for simple lights setup (these colours and modes will stay as default until AWCC run or modified by another app), even after reboot.  
You can also assign event for light to react on (power state, performance indicator, or just activity light), as well as a colour for reaction at "Monitoring" tab.  
Tray menu (right-click on tray button) available all the time for some fast switch functions, application hide to tray completely then minimized.  
```
How it works
```
![Color tab](https://github.com/T-Troll/alienfx-tools/tree/master/Doc/img/gui-color.png)

`"Colour"` tab is set hardware colours and effects for light. This setting will remain even if you exit application.  

Each light (except Power Button) can have up to 9 different hardware effects assigned to it, but some modes require more, then one effect (f.e. Morph � 2, Spectrum � 7) to work correctly.  
Use "Effects" list to add/remove/select effect. For each effect, you can define its colour, effect mode, speed (how fast to change the colour), and length (how long it plays).  
Available effect modes are:
- Colour - Stay at solid colour defined.
- Pulse - Switch between defined colour and black.
- Morph - Morph light from previous colour to current one.
- Breath - Morph light from black to current colour. (For devices with APIv4)
- Spectrum - Like a morph, but up to 9 colours can be defined. (For devices with APIv4)
- Rainbow - Like a Colour, but can use up to 9 colours. (For devices with APIv4)

Please keep in mind, mixing different event modes for one light can provide unexpected results, as well as last 2 modes can be unsupported for some lights (will do nothing). But you can experiment.  
�Set All� button copies current light effects to all lights into the list (it�s useful if you need to have all lights into the same colour and mode).  

![Monitoring tab](https://github.com/T-Troll/alienfx-tools/tree/master/Doc/img/gui-monitoring.png)

`"Monitoring"` tab designed for system events monitoring and change lights to reflect it - like power events, system load, temperatures.  
There are some different monitoring types available:  

"Light for colour and monitoring"  
If this mode is active, selected light will use colours from "Colour" tab as initial monitoring colour. If this mode disabled, light will use colour from "Monitoring" tab, and also will stop update if monitoring off.  

"Power State"  
Light acts as software power button, and reflect current power mode - AC, battery, charge, low battery level.  

"Performance"  
Light acts like a performance indicator, reflecting system parameters:
- CPU Load - CPU load colour mix from 0% ("calm") to 100% ("Active")
- RAM Load - The same for used RAM percentage
- GPU Load - The same for utilized GPU percentage (top one across GPUs if more, then one present into the system).
- HDD Load - It's not exactly a load, but IDLE time. If idle - it's "calm", 100% busy - active, and mix between.
- Network load - Current network traffic value against maximal value detected (across all network adapters into the system).
- Max. Temperature - Maximal temperature in Celsius degree across all temperature sensors detected into the system.
- Battery level - Battery charge level in percent (100=discharged, 0=full).
- Max. Fan RPM - Maximal RPM (in percent of maximal) across all system fans. This indicator will only work if "Fan control" is enabled.
  
You can use "Minimal value" slider to define zone of no reaction - for example, for temperature it's nice to set it to the room temperature - only heat above it will change colour.  
"Gauge" checkbox changes behaviour for groups only. If Gauge on, all lights in group works as a level indicator (100% colour below indicator value, 0% colour above indicator value, mixed in between.

"Event" - Light switches between colours if system event occurs:
- Disk activity - Switch light every disk activity event (HDD IDLE above zero).
- Network activity - Switch light if any network traffic detected (across all adapters).
- System overheat - Switch light if system temperature above cut level (default 95C, but you can change it using slider below).
- Out of memory - Switch light if memory usage above 90% (you can change it by the same slider).
- Low battery - Switch light if battery charged below the level defined by slider.
  
"Blink" checkbox switch triggered value to blink between on-off colours 6 times per sec.
You can mix different monitoring type at once, f.e. different colours for same light for both CPU load and system overheat event. In this case Event colour always override Performance one then triggered, as well as both override Power state one.

![Devices tab](https://github.com/T-Troll/alienfx-tools/tree/master/Doc/img/gui-devices.png)

`"Devices"` tab is an extended GUI for `Alienfx-probe`, providing devices and lights names and settings, name modification, light testing and some other hardware-related settings.  

"Devices" dropdown shows the list of the light devices found into the system, as well as selected device status (ok/error), you can also edit their names here.  
"Reset" button refresh the devices list (useful after you disconnect/connect new device), as well as re-open each device in case it stuck.  
"Lights" list shows all lights defined for selected device. Use �Add�/�Remove� buttons to add new light or remove selected one.  
NB: If you want to add new light, type light ID into LightID box **before** pressing �Add� button. If this ID already present in list or absent, it will be changed to the first unused ID.  
Double-click or press Enter on selected light to edit its name.  
"Reset light" button keep the light into the list, but removes all settings for this light from all profiles, so it will be not changed anymore until you set it up again.  
"Power button" checkbox set selected light as a "Hardware Power Button". After this operation, it will react to power source state (ac/battery/charging/sleep etc) automatically, but this kind of light change from the other app is a dangerous operation, and can provide unpleasant effects or light system hang.  
"Indicator" checkbox is for indicator lights (HDD, caps lock, Wi-Fi) if present. Then checked, it will not turn off with screen/lights off (same like power), as well as will be disabled in other apps.
Selected light changes it colours to the one defined by "Test colour" button, and fade to black then unselected.

"Load Mappings" button loads pre-defined lights map (if exist) for you gear. Map files is a simple .csv defining all devices, its names, lights and its types and names. Useful for first start.  
"Save Mappings" button save current active devices and their lights into .csv file. Please, send me this file if your device is not into pre-defined mappings list yet, I�ll add it.

![Groups tab](https://github.com/T-Troll/alienfx-tools/tree/master/Doc/img/gui-groups.png)

`"Groups"` tab provides control for light groups. Each group can be selected and set at "Colour" and "Monitoring" pages as a one light.  
Press [+] and [-] buttons to add/remove group, use dropdown to select it or change its name.  
If you have a group selected, "Group lights" list present the list of lights assigned to this group. Use [-->] and [<--] buttons to add and remove light from the group.  

![Profiles tab](https://github.com/T-Troll/alienfx-tools/tree/master/Doc/img/gui-profiles.png)

`"Profiles"` tab control profile settings, like selecting default profile, per-profile monitoring control and automatic switch to this profile then the defined application run.  

You can double-click or press Enter on selected profile into the list to edit its name.  
Each profile can have settings and application for trigger it. The settings are:
- "Application" - Defines application executable for trigger profile switch if "Profile auto switch" enabled.
- "Default profile" - Default profile is the one used if "Profile auto switch" enabled, but running applications doesn't fit any other profile. There is can be only one Default profile, and it can't be deleted.
- "Disable monitoring" - Then profile activated, monitoring functions are disabled, despite of global setting.
- "Dim lights" - Then profile activated, all lights are dimmed to common amount.
- "Only then active" - If "Profile auto switch" enabled, and application defined in profile running, profile will only be selected if application window active (have focus).
- "Fan settings" - If selected, profile also keep fan control settings and restore it then activated. 

![Fans tab](https://github.com/T-Troll/alienfx-tools/tree/master/Doc/img/gui-fans.png)

`"Fans"` tab is UI for control system fans and temperature sensors.  

First, take a look at "Power mode" dropdown - it can control system pre-defined power modes. For manual fan control switch it to "Manual".  
"Temperature sensors" list present all hardware sensors found at your motherboard (some SSD sensors can absent into this list), and their current temperature values.
"Fans" list presents all fans found into the system and their current RPMs.  
"GPU limit" slider define top maximal GPU board power limit shift. 0 (left) is no limit, 4 (right) - maximal limit. This feature only supported at some notebooks, and real limit can vary. You can use this slider to keep more power at CPU or extend battery life.

Additional "Fan curve" window at the right shows currently selected fan temperature/boost curve, as well as current boost.  

Fan control is temperature sensor-driven, so first select one of temperature sensors.  
Then, select which fan(s) should react on its readings - you can select from none to all in any combination.  
So, select checkbox for fan(s) you need.

After you doing so, currently selected fan settings will be shown at "Fan Curve" window - you will see current fan boost at the top, and the fan control curve (green lines).
Now play with fan control curve - it defines fan boost by temperature level. X axle is temperature, Y axle is boost level.  
You can left click (and drag until release mouse button) into the curve window to add point or select close point (if any) and move it.  
You can click right mouse button at the graph point to remove it.  
Big red dot represents current boost-in-action position.  

Please keep in mind:
- You can't remove first or last point of the curve.
- If you move first or last point, it will keep its temperature after button release - but you can set other boost level for it.
- Then fan controlled by more, then one sensor, boost will be set to the maximal value across them. 

![Settings tab](https://github.com/T-Troll/alienfx-tools/tree/master/Doc/img/gui-settings.png)

`"Settings"` tab is for application/global lights settings control - states, behaviour, dimming, as well as application settings:
- "Turn on lights" - Operate all lights into the system. It will be black if this option disabled (default - on).
- "Lights follow screen state" - Dim/Fade to black lights then system screen dimmed/off (default - off).
- "Power/indicator lights too" - Lights, marked as Power Button or Indicator follows the system state. Such lights will be always on if disabled (default - off).
- "Auto refresh lights" - All lights will be refreshed 6 times per second. It's useful if you have AWCC running, but still want to control lights (default - off).
- "Colour Gamma correction" - Enables colour correction to make them looks close to screen one. It keeps original AWCC colours if disabled (default - on).
- "Dim lights" - Dim system lights brightness. It's useful for night/battery scenario (default - off).
- "Dim Power/Indicator lights too" - Lights, marked as Power Button or Indicator follows dim state. Such lights will be always at full brightness if disabled (default - off).
- "Dim lights on battery" - Automatically dim lights if system running at battery power, decreasing energy usage (default - on).
- "Dimming power" - Amount of the brightness decrease then dimmed. Values can be from 0 to 255, default is 92.
- "Start with Windows" - Start application at Windows start. It will not work if application request run as admin level (see below) (default - off).
- "Start minimized" - Hide application window in system tray after start.
- "Enable monitoring" - Application start to monitor system metrics (CPU/GPU/RAM load, etc) and refresh lights according to it (default - on).
- "Profile auto switch" - Switch between profiles available automatically, according of applications start and finish. This also block manual profile selection (default - off).
- "Disable AWCC" - Application will check active Alienware Control Centre service at the each start and will try to stop it (and start back upon exit). It will require "Run as administrator" privilege (default - off).
- "Esif temperature" - Read hardware device temperature counters. If disabled, only system-wide ones will be read. It's useful for some Dell and Alienware systems, but also provide a lot of component temperature readings. It will require "Run as administrator" privilege (default - off).
- "Enable fan control" - Enables all fan control functionality, if possible.

"Global effects" block provides global control for light effects for current device. It's only work for APIv5 devices, provide some cool animations across all keys. 

Keyboard shortcuts (any time):
- CTRL+SHIFT+F12 - enable/disable lights
- CTRL+SHIFT+F11 - dim/undim lights
- CTRL+SHIFT+F10 - enable/disable system state monitoring
- F18 (on Alienware keyboards it's mapped to Fn+AlienFX) - cycle light mode (on-dim-off)

Other shortcuts (only then application active):
- ALT+1 - switch to "Colours" tab
- ALT+2 - switch to "Monitoring" tab
- ALT+3 - switch to "Devices and Lights" tab
- ALT+4 - switch to "Profiles" tab
- ALT+5 - switch to "Settings" tab
- ALT+r - refresh all lights
- ALT+m - minimize app window
- ALT+s - save configuration
- ALT+? - about app
- ALT+x - quit

**WARNING:** All hardware colour effects stop working if you enable any Event monitoring. It�s a hardware bug � any light update operation restarts all effects.  