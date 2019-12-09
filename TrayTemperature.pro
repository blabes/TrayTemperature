HEADERS       = \
    TrayTemperature.h
SOURCES       = \
                TrayTemperature.cpp \
                main.cpp
RESOURCES     = \
    TrayTemperature.qrc
FORMS += \
    TrayTemperatureAbout.ui \
    TrayTemperatureConfig.ui

QT += widgets network

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    ../../Downloads/c-weather-cloudy-with-sun.ico \
    LICENSE.txt \
    README.md \
    TrayTemperature.iss \
    c-weather-cloudy-with-sun.ico \
    gitversion.pri \
    post-install-notes.rtf

RC_ICONS = c-weather-cloudy-with-sun.ico
include(gitversion.pri)
