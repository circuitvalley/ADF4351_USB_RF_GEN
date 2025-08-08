#include "hid_pnp.h"

HID_PnP::HID_PnP(QObject *parent) : QObject(parent) {
    ui_data.isConnected = false;

    ui_data.ioUpdatePending = 0;

    device = NULL;
    buf[0] = 0x00;
    buf[1] = 0x37;
    memset((void*)&buf[2], 0x00, sizeof(buf) - 2);
    memset(ui_data.adf4351.reg, 0, sizeof(ui_data.adf4351.reg));

    usbDeviceTimer = new QTimer();
    connect(usbDeviceTimer, SIGNAL(timeout()), this, SLOT(FindUSBDevices ()));
    usbDeviceTimer->start(1000);

    timer = new QTimer();
    connect(timer, SIGNAL(timeout()), this, SLOT(PollUSB()));

    timer->start(250);

    slow_read = new QTimer();
    connect(slow_read, SIGNAL(timeout()), this, SLOT(slow_read_timeout()));
    slow_read->start(500);
}

HID_PnP::~HID_PnP() {
    disconnect(timer, SIGNAL(timeout()), this, SLOT(PollUSB()));
}

void HID_PnP::FindUSBDevices()
{

    struct hid_device_info *devices, *current;

    if(!(ui_data.isSelctedDeviceChange == false or ui_data.isConnected == false)) // find use device is no current change is pending to prevent race condition
    {
        return;
    }

    ui_data.usb_device_list.clear();
    devices = hid_enumerate(0x1209, 0x7877);



    if (ui_data.isConnected == true and ui_data.selected_usb_device.length() > 0)  // force append first item only if connected as it would not found if connected.
    {
        ui_data.usb_device_list.append(ui_data.selected_usb_device);
    }
    else
    {
        if (!devices) {
            ui_data.usb_device_list.append("No Device Found");
            //return;
        }
    }
    current = devices;

    while (current) {
        QString deviceName = QString("%1").arg(QString::fromWCharArray(current->serial_number));

        ui_data.usb_device_list.append(deviceName);

        current = current->next;
    }

    ui_data.usb_device_list.sort();

    if ( ui_data.last_usb_device_list !=  ui_data.usb_device_list)
    {
        ui_data.isDeviceListChanged = true;
        hid_comm_update(ui_data.isConnected, &ui_data);
    }
    else
    {
        ui_data.isDeviceListChanged = false;
    }
    ui_data.last_usb_device_list = ui_data.usb_device_list;
}

void HID_PnP::PollUSB()
{
    buf[0] = 0x00;
    memset((void*)&buf[2], 0x00, sizeof(buf) - 2);

    if (ui_data.isConnected == false) {
        hid_comm_update(ui_data.isConnected, &ui_data);
        size_t len = static_cast<size_t>(ui_data.selected_usb_device.length());  // Explicit cast to size_t
        if( len > 0) //open only if any use device is selected.
        {
            wchar_t* device_serial = new wchar_t[len + 1];
            ui_data.selected_usb_device.toWCharArray(device_serial);
            device_serial[len] = L'\0';
            device = hid_open(0x1209, 0x7877, device_serial);  //vid pid

            if (device) {
                ui_data.isConnected = true;
                ui_data.isSelctedDeviceChange = false;
                hid_set_nonblocking(device, true);
                timer->start(15);
            }
        }
    }
    else {

        if (ui_data.isSelctedDeviceChange == true)
        {
            CloseDevice();
            ui_data.isSelctedDeviceChange = false;
            ui_data.isDeviceChangeDone = true;
            return;
        }

        if(ui_data.isRegUpdatePending == true)
        {
            ui_data.isRegUpdatePending = false;

            buf[1] = COMMAND_SET_REG ;
            memcpy(&buf[2], &ui_data.adf4351, sizeof(ui_data.adf4351));

            if (hid_write(device, buf, sizeof(buf)) == -1)
            {
                CloseDevice();
                return;
            }
        }

        if(ui_data.isRF_CTRL_Pending == true)
        {
            ui_data.isRF_CTRL_Pending = false;
            buf[1] = COMMAND_RF_CTRL ;
            buf[2] = ui_data.RF_OUT ;
            if (hid_write(device, buf, sizeof(buf)) == -1)
            {
                CloseDevice();
                return;
            }
        }

        if (ui_data.isDeviceCtrlPending == true)
        {
            ui_data.isDeviceCtrlPending = false;
            hid_comm_update(ui_data.isConnected, &ui_data); // get latest from ui
            buf[1] = COMMAND_DEVICE_CTRL ;
            buf[2] = ui_data.isFlashWriteRequested;
            buf[3] = ui_data.isIdentfiyLEDRequested;
            buf[4] = 0x0;   // erase
            memcpy(&buf[5], &ui_data.adf4351, sizeof(ui_data.adf4351));
            ui_data.isIdentfiyLEDRequested = false;
            ui_data.isFlashWriteRequested   = false;

            if (hid_write(device, buf, sizeof(buf)) == -1)
            {
                CloseDevice();
                return;
            }
        }

        if (ui_data.isWriteSerialNumberRequested == true)
        {
            hid_comm_update(ui_data.isConnected, &ui_data); // get latest from ui
            buf[0] = 0x00;
            buf[1] = COMMAND_SET_SERIAL_INFO;
            ui_data.isWriteSerialNumberRequested = false;
            memcpy(&buf[2], &ui_data.deviceinfo, sizeof(ui_data.deviceinfo));
            if (hid_write(device, buf, sizeof(buf)) == -1)
            {
                CloseDevice();
                return;
            }

        }

        if (ui_data.isEraseFlashRequested == true)
        {
            ui_data.isEraseFlashRequested = false;
            hid_comm_update(ui_data.isConnected, &ui_data); // get latest from ui
            buf[1] = COMMAND_DEVICE_CTRL ;
            buf[2] = 0x01; //force flash write
            buf[3] = 0x01; //force blink status LED
            buf[4] = 0x01; //Erase Flash
            memset(&buf[5], 0xFF, sizeof(buf) - 5);

            if (hid_write(device, buf, sizeof(buf)) == -1)
            {
                CloseDevice();
                return;
            }
        }


        if ( ui_data.isReadFirmwareInfoPending == true )
        {
            buf[0] = 0x00;
            buf[1] = COMMAND_GET_BUILD_INFO;
            memset(static_cast<uint8_t *>(&buf[2]), 0x00, sizeof(buf) - 2);
            if (hid_write(device, buf, sizeof(buf)) == -1)
            {
                CloseDevice();
                return;
            }
            if(hid_read(device, buf, sizeof(buf)) == -1)
            {
                CloseDevice();
                return;
            }
        }

        if (ui_data.readRFCTRL_pending)
        {
            buf[0] = 0x00;
            buf[1] = COMMAND_READ_RF_CTRL;
            memset(static_cast<uint8_t *>(&buf[2]), 0x00,sizeof(buf) - 2);
            if (hid_write(device, buf, sizeof(buf)) == -1)
            {
                CloseDevice();
                return;
            }

            if(hid_read(device, buf, sizeof(buf)) == -1)
            {
                CloseDevice();
                return;
            }
        }

        if (buf[0] == COMMAND_READ_RF_CTRL)
        {
            ui_data.readRFCTRL_pending = false;
            ui_data.RF_OUT = (bool) buf[1];
            ui_data.device_busy = (bool) buf[2];
            hid_comm_update(ui_data.isConnected, &ui_data);

        }
        else if (buf[0] == COMMAND_GET_BUILD_INFO)
        {
            ui_data.isReadFirmwareInfoPending = false;
            ui_data.firmware_version_major = buf[ 1 ];
            ui_data.firmware_version_minor = buf[ 2 ];
            ui_data.firmware_build_number = (buf[ 3 ] << 8) | buf[ 4 ];
            hid_comm_update(ui_data.isConnected, &ui_data);
        }
    }
}



void HID_PnP::change_reg(const uint32_t *reg, bool isStartOfSweep)
{
    memcpy(ui_data.adf4351.reg, reg, sizeof(ui_data.adf4351.reg));
    ui_data.isRegUpdatePending    = true;
    ui_data.adf4351.isStartOfSweep = isStartOfSweep;
}


void HID_PnP::change_RF_CTRL()
{
    ui_data.RF_OUT = !ui_data.RF_OUT;
    ui_data.isRF_CTRL_Pending= true;
    hid_comm_update(ui_data.isConnected, &ui_data);

}

void HID_PnP::CloseDevice()
{
    hid_close(device);
    device = NULL;
    ui_data.isConnected = false;
    ui_data.ioUpdatePending = 0;
    ui_data.selected_usb_device.clear();
    hid_comm_update(ui_data.isConnected, &ui_data);
    timer->start(250);

}


void HID_PnP::slow_read_timeout()
{
    ui_data.isReadFirmwareInfoPending = true;
    ui_data.readRFCTRL_pending  = true;
}
