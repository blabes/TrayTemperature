HEADERS       = \
    TrayTemperature.h
SOURCES       = \
                TrayTemperature.cpp \
                main.cpp
RESOURCES     = \
    TrayTemperature.qrc
FORMS += \
    TrayTemperatureConfig.ui

QT += widgets network

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    README.md \
    TrayTemperature.iss \
    post-install-notes.rtf \
    post-install-notes.txt
