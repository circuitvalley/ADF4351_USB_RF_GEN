#-------------------------------------------------
# Project created by QtCreator 2010-10-28T17:06:16
#-------------------------------------------------

QT       += core gui
QT += gui widgets
TARGET = CV_ADF4351
TEMPLATE = app


SOURCES += main.cpp\
        hid_pnp.cpp \
    usbioboard.cpp \
    adf4351.cpp

HEADERS  += \
         hid_pnp.h \
    usbioboard.h \
    adf4351.h


FORMS    += \
    usbio.ui

RC_FILE = app.rc
#-------------------------------------------------
# Add the Signal11's hidapi library that was
# created
#-------------------------------------------------
macx: LIBS += -L../HIDAPI/mac -lHIDAPI
win32: LIBS += -L../HIDAPI/windows -lHIDAPI
unix: !macx: LIBS += -L../HIDAPI/linux -lHIDAPI

#-------------------------------------------------
# Make sure to add the required libraries or
# frameoworks for the hidapi to work depending on
# what OS is being used
#-------------------------------------------------
macx: LIBS += -framework CoreFoundation -framework IOkit
win32: LIBS += -lSetupAPI
unix: !macx: LIBS += -lusb-1.0

#-------------------------------------------------
# Make sure output directory for object file and
# executable is in the correct subdirectory
#-------------------------------------------------
macx {
    DESTDIR = mac
    OBJECTS_DIR = mac
    MOC_DIR = mac
    UI_DIR = mac
    RCC_DIR = mac
}
unix: !macx {
    DESTDIR = linux
    OBJECTS_DIR = linux
    MOC_DIR = linux
    UI_DIR = linux
    RCC_DIR = linux
}
win32 {
    DESTDIR = windows
    OBJECTS_DIR = windows
    MOC_DIR = windows
    UI_DIR = windows
    RCC_DIR = windows
}

OTHER_FILES +=

RESOURCES += \
    resources.qrc
