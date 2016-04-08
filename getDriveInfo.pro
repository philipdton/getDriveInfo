QT += widgets

HEADERS       = button.h \
    getDriveInfo.h \
    scsideviceio.h
SOURCES       = button.cpp \
                main.cpp \
    getDriveInfo.cpp \
    ScsiDeviceIO.cpp

# install
target.path = C:/dev/getDriveInfo
INSTALLS += target
