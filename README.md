TrayTemperature
===============

  7-Dec-2019: first release  
 11-Dec-2019: 1.2.0 - added options to display degree symbol and autosize font

# Summary
- [Description](#description)
- [Development Platform](#development-platform)
- [Installation on Windows](#installation-on-windows)
- [Installation on Other Platforms](#installation-on-other-platforms)
- [Application Screenshots](#screenshots)

# Description
A Qt Dialog application implemented in C# to display the current outdoor temperature in the Windows notification area, somtimes referred to as the system tray.  The computer's location is found using [http://ip-api.com/](http://ip-api.com/) so if your IP address is not a good clue to your real location, you'll need to determine and enter your latitude and longitude by hand.  Temperature information is retrieved from [OpenWeatherMap](http://openweathermap.org/). In order to make use of OpenWeatherMap, you'll need to register on their site and obtain a free API key, which you then copy and paste into this application's configuration screen on first use.

I took inspriation from the following Qt programs and examples: [TrayWeather](https://github.com/FelixdelasPozas/TrayWeather) by Félix de las Pozas Álvarez, [Qt System Tray Icon Example](https://doc.qt.io/qt-5/qtwidgets-desktop-systray-example.html), and [Qt Weather Info Example](https://doc.qt.io/qt-5/qtpositioning-weatherinfo-example.html).

## Options
The temperature display units (&deg;F or &deg;C), the update frequency, font, and color are all configurable. You can also choose whether to allow ip-api.com to do its best to determine your latitude and longitude based on your IP address, or you can enter your latitude and longitude manually.  As of ![version 1.2.0](https://github.com/blabes/TrayTemperature/releases/tag/1.2.0), you can choose whether or not to display a degree symbol after the temperature, and you can choose your own font size or have the program autosize the font to fit into the icon.

# Development Platform
This application was developed in C++ in Qt on Microsoft Windows 10 using [Qt Creator 4.10.2](https://www.qt.io/development-tools). It was built with the MinGW 64-bit compiler.

# Installation on Windows
Download the latest ![release](https://github.com/blabes/TrayTemperature/releases/) installer and run it.

# Installation on Other Platforms
For now you'll have to build it yourself.  Grab Qt Creator and go to town.

# Application Screenshots
- The temperature  
![Temperature only](/../screenshots/TrayTemperature%20taskbar.png?raw=true "Temperature only")
![Temperature with degree symbol](/../screenshots/TrayTemperature%20tooltip%20degree.png?raw=true "Temperature with degree symbol") 

- Temperature and its tooltip  
![Temperature and tooltip](/../screenshots/TrayTemperature%20tooltip.png?raw=true "Temperature and tooltip")  

- Configuration screen  
![Config Screen](/../screenshots/TrayTemperature%20config%201_2_0.png?raw=true "Config Screen")  
