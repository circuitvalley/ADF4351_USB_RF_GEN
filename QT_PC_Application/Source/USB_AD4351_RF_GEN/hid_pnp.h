#ifndef HID_PNP_H
#define HID_PNP_H

#include <QObject>
#include <QTimer>
#include "../HIDAPI/hidapi.h"

#include <wchar.h>
#include <string.h>
#include <stdlib.h>

#define MAX_ADC_CHANNEL 8  //including tempsensor
#define MAX_STR 65

#define REF_VREF_PIN     4 //has nothing to do with FVR but still hier
#define REF_4v096   3
#define REF_2v048   2
#define REF_1v025   1

typedef enum
{
      COMMAND_SET_REG = 0x80,  			//writes current registers to the device's ram
      COMMAND_GET_REG = 0x83,  			//reads current register from device's ram
      COMMAND_RF_CTRL = 0x81,			//control rf output, turn off or on
      COMMAND_READ_RF_CTRL = 0x82,		//read current state of rf output
      COMMAND_DEVICE_CTRL  = 0x84,      //write flash or device identify
      COMMAND_SET_SERIAL_INFO = 0x85,   //write serial number
      COMMAND_GET_BUILD_INFO = 0xB0,	//read current fw version and build number

} CUSTOM_HID_DEMO_COMMANDS;



class UI_Data
{
    public:
    bool isConnected;
    bool isDeviceCtrlPending = false;
    bool ioUpdatePending = true;
    bool pwdUpdatePending = true;
    bool isRegUpdatePending = false;
    bool isRF_CTRL_Pending = true;
    bool isReadFirmwareInfoPending = true;
    bool isWriteSerialNumberRequested = false;
    bool isEraseFlashRequested = false;
    struct deviceinfo_s{
        uint32_t    serialNumber;
    }deviceinfo;

    uint8_t readRFCTRL_pending = true;
    uint16_t firmware_build_number;
    uint8_t firmware_version_major;
    uint8_t firmware_version_minor;
    QStringList last_usb_device_list;
    QStringList usb_device_list;
    bool  isDeviceListChanged = false;
    QString selected_usb_device;

    struct adf4351_s{
        uint32_t        reg[6];             //24 bytes
        uint32_t        frequency;          // 4 bytes
        uint32_t        ref_freq;           // 4 bytes
        uint32_t        start_freq;         // 4 bytes
        uint32_t        stop_freq;          // 4 bytes
        uint32_t        step_freq;          // 4 bytes
        uint16_t        step_ms;            // 2 bytes
        uint16_t        aux_select;         // 2 byte
        uint8_t         isSweepEnabled;     // 1 byte
        uint8_t         isStartOnBoot;      // 1 byte
        uint8_t         isStartOfSweep;     // 1 byte
        uint8_t         flashWritePending;  // 1byte //place holder
        uint8_t         flashReadPending;   // 1byte  // place holder
    }adf4351;

    bool isSelctedDeviceChange = false;
    bool isDeviceChangeDone = false;
    bool RF_OUT = false;
    bool device_busy = false;
    bool writeToFalsh = false;
    bool autoStartatBoot = false;
    bool isIdentfiyLEDRequested = false;
    bool isFlashWriteRequested = false;
};

class HID_PnP : public QObject
{
    Q_OBJECT
public:
    explicit HID_PnP(QObject *parent = nullptr);
    ~HID_PnP();


signals:
    void hid_comm_update(bool isConnected, UI_Data *ui_data);

public slots:
    void PollUSB();
    void change_reg(const uint32_t *regm, bool isStartOfSweep);
    void change_RF_CTRL();
    void slow_read_timeout();
    void FindUSBDevices();

private:

    UI_Data ui_data;

    hid_device *device;
    QTimer *usbDeviceTimer;
    QTimer *timer;
    QTimer *slow_read;
    uint8_t buf[MAX_STR];

    void CloseDevice();
};

#endif // HID_PNP_H
