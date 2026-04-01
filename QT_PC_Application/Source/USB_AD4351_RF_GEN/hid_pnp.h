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
      COMMAND_SET_REG = 0x80,           //writes current registers to the device's ram
      COMMAND_GET_REG = 0x83,           //reads current register from device's ram
      COMMAND_RF_CTRL = 0x81,           //control rf output, turn off or on
      COMMAND_READ_RF_CTRL = 0x82,      //read current state of rf output
      COMMAND_DEVICE_CTRL  = 0x84,      //write flash or device identify
      COMMAND_SET_SERIAL_INFO = 0x85,   //write serial number
      COMMAND_SET_MACRO = 0x86,         //write macro block (0 or 1) to device
      COMMAND_GET_MACRO = 0x87,         //read macro block (0 or 1) from device
      COMMAND_GET_BUILD_INFO = 0xB0,    //read current fw version and build number

} CUSTOM_HID_DEMO_COMMANDS;

/* Status flag bit definitions - must match firmware */
#define STEP_FLAG_ENABLED       0x01
#define STEP_FLAG_RF_MASK       0x06
#define STEP_FLAG_RF_SHIFT      1
#define STEP_FLAG_RF_ON         0x01
#define STEP_FLAG_RF_OFF        0x02
#define STEP_FLAG_RECALC_PLL    0x08

/* Packed structs matching firmware layout (uint24_t = 3 bytes LE) */
#pragma pack(push, 1)
struct macroStep_s {
    uint8_t frequency[3];   // uint24_t LE, units 0.01 MHz
    uint8_t step_ms[3];     // uint24_t LE, milliseconds
    uint8_t status_flag;
};

struct devicemacro_s {
    macroStep_s stepConfig[7];  // 49 bytes
    uint8_t     macroSteps;     // 1 byte
    uint8_t     padding[3];     // 3 bytes  = 53 total
};
#pragma pack(pop)

static inline void packUint24LE(uint8_t *dest, uint32_t val) {
    dest[0] = val & 0xFF;
    dest[1] = (val >> 8) & 0xFF;
    dest[2] = (val >> 16) & 0xFF;
}

static inline uint32_t unpackUint24LE(const uint8_t *src) {
    return (uint32_t)src[0] | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16);
}


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

    #pragma pack(push, 1)
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
        uint8_t         flashWritePending;  // 1 byte  // place holder
        uint8_t         flashReadPending;   // 1 byte  // place holder
        uint8_t         isMacroEnabled;     // 1 byte
    }adf4351;                               // 56 bytes total — matches firmware ADF4351_reg_t
    #pragma pack(pop)

    /* Macro data - two blocks of 7 steps each */
    devicemacro_s deviceMacro[2];
    bool isMacroWritePending = false;

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
    void set_RF_state(bool state);
    void write_macro();
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
