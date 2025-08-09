import argparse
import hid
import sys
import struct
import math
import ctypes

TARGET_VID = 0x1209
TARGET_PID = 0x7877
COMMAND_SET_REG = 0x80
COMMAND_GET_REG = 0x83
COMMAND_RF_CTRL = 0x81
COMMAND_READ_RF_CTRL = 0x82
COMMAND_DEVICE_CTRL  = 0x84
COMMAND_GET_BUILD_INFO = 0xB0


class ADF4351_reg_t(ctypes.Union):
    class _Regs(ctypes.Structure):
        _fields_ = [
            ("reg", ctypes.c_uint32 * 6),         # 24 bytes
            ("frequency", ctypes.c_uint32),       # 4 bytes
            ("ref_freq", ctypes.c_uint32),        # 4 bytes
            ("start_freq", ctypes.c_uint32),      # 4 bytes
            ("stop_freq", ctypes.c_uint32),       # 4 bytes
            ("step_freq", ctypes.c_uint32),       # 4 bytes
            ("step_ms", ctypes.c_uint16),         # 2 bytes
            ("aux_select", ctypes.c_uint16),      # 2 bytes
            ("isSweepEnabled", ctypes.c_uint8),   # 1 byte
            ("isStartOnBoot", ctypes.c_uint8),    # 1 byte
            ("isStartOfSweep", ctypes.c_uint8),   # 1 byte
            ("flashWritePending", ctypes.c_uint8),# 1 byte
            ("flashReadPending", ctypes.c_uint8), # 1 byte
            ("sanityCheck", ctypes.c_uint16),     # 2 bytes
            ("padding", ctypes.c_uint8 * 1),      # 1 byte padding
        ]

    _anonymous_ = ("regs",)
    _fields_ = [
        ("regs", _Regs),
        ("mem", ctypes.c_uint8 * 56)
    ]



class AD4351:
    def __init__(self):
        # Public members
        self.REF_FREQ = 25.0 
        self.ref_doubler = False
        self.ref_div2 = False
        self.enable_gcd =  True
        self.feedback_select = True  # default 1 funda metal
        self.band_select_clock_mode = False # low
        self.clock_divider = 150
        self.band_select_clock_freq = 0.0
        self.band_select_auto = True  #issue
        self.N = 0.0
        self.PFDFreq = 0.0
        self.PHASE_ADJUST = False #0 off
        self.PR1 = True #issue should it be bool? default value 1
        self.low_noise_spur_mode = 0  # uint8_t 0 low noise 1,2 Reserved 3 Low Supr
        self.muxout = 0  # uint8_t # 0 3state
        self.charge_pump_current = 7  # uint8_t default 8 2.81
        self.LDF = False #Frac-N
        self.LDP = False #10ns
        self.PD_Polarity = True # positive
        self.power_down = False # 0 disabled 
        self.cp_3stage = False # 0 disabled
        self.counter_reset = False # disabled
        self.double_buff = False #disable
        self.ABP = False # 6ns Frac-N
        self.charge_cancelletion = False #disabled
        self.CSR = False #disabled
        self.CLK_DIV_MODE = 0  # uint8_t clock div off
        self.LD = 1  # uint8_t default 1 digitabl lock detect
        self.VCO_power_down = False #disabled
        self.MTLD = False #disbaled 
        self.AUX_output_mode = False # 0 Aux disable
        self.AUX_output_enable = False # Aux out disbled
        self.AUX_output_power = 0  # uint8_t 0 - 4dbm
        self.RF_output_power = 3  # uint8_t default 3 (+5 dBm)
        self.RF_ENABLE = True
        self.PHASE = 1
        self.r_counter = 50
        self.frequency = 100.0
        self.INT = 0
        self.MOD = 0.0
        self.FRAC = 0.0
        self.band_select_clock_divider = 0
        self.registers = ADF4351_reg_t()
        
    def AD4351_calculte_reg_from_freq(self, frequency: int):
        # Placeholder for your method
        pass

    @staticmethod
    def gcd(a: int, b: int) -> int:
        a &= 0xFFFFFFFF  # Ensure a is 32-bit unsigned
        b &= 0xFFFFFFFF  # Ensure b is 32-bit unsigned
        while True:
            if a == 0:
                return b & 0xFFFFFFFF
            elif b == 0:
                return a & 0xFFFFFFFF
            elif a > b:
                a = a % b
            else:
                b = b % a

    def BuildRegisters(self):
        self.PFDFreq = (self.REF_FREQ * (2 if self.ref_doubler else 1) / (2 if self.ref_div2 else 1) / self.r_counter)

        output_divider = 1
        if self.frequency >= 2200.0:
            output_divider = 1
        if self.frequency < 2200.0:
            output_divider = 2
        if self.frequency < 1100.0:
            output_divider = 4
        if self.frequency < 550.0:
            output_divider = 8
        if self.frequency < 275.0:
            output_divider = 16
        if self.frequency < 137.5:
            output_divider = 32
        if self.frequency < 68.75:
            output_divider = 64

        if self.feedback_select:
            self.N = self.frequency * output_divider / self.PFDFreq
        else:
            self.N = self.frequency / self.PFDFreq

        self.INT = int(self.N)
        self.MOD = int(round(1000 * self.PFDFreq))
        self.FRAC = int(round((self.N - self.INT) * self.MOD))
        
        if self.enable_gcd:
            div = self.gcd(self.MOD, self.FRAC)
            self.MOD //= div
            self.FRAC //= div

        if self.MOD == 1:
            self.MOD = 2

        if self.band_select_auto:
            if self.band_select_clock_mode == 0:
                temp = int(round(8.0 * self.PFDFreq))
                if (8.0 * self.PFDFreq - temp) > 0:
                    temp += 1
                temp = min(temp, 255)
            else:
                temp = int(round(2.0 * self.PFDFreq))
                if (2.0 * self.PFDFreq - temp) > 0:
                    temp += 1
                temp = min(temp, 255)
            self.band_select_clock_divider = temp
            self.band_select_clock_freq = 1000 * self.PFDFreq / temp


       # Register calculations
        self.registers.reg[0] = ((self.INT & 0xFFFF) << 15) + ((self.FRAC & 0xFFF) << 3) + 0
        self.registers.reg[1] = (self.PHASE_ADJUST << 28) + (self.PR1 << 27) + (self.PHASE << 15) + ((self.MOD & 0xFFF) << 3) + 1
        self.registers.reg[2] = (self.low_noise_spur_mode << 29) + (self.muxout << 26) + ((1 if self.ref_doubler else 0) << 25) + ((1 if self.ref_div2 else 0) << 24) + (self.r_counter << 14) + (self.double_buff << 13) + (self.charge_pump_current << 9) + (self.LDF << 8) + (self.LDP << 7) + (self.PD_Polarity << 6) + (self.power_down << 5) + (self.cp_3stage << 4) + (self.counter_reset << 3) + 2
        self.registers.reg[3] = (self.band_select_clock_mode << 23) + (self.ABP << 22) + (self.charge_cancelletion << 21) + (self.CSR << 18) + (self.CLK_DIV_MODE << 15) + (self.clock_divider << 3) + 3
        self.registers.reg[4] = (self.feedback_select << 23) + (int(math.log2(output_divider)) << 20) + (self.band_select_clock_divider << 12) + (self.VCO_power_down << 11) + (self.MTLD << 10) + (self.AUX_output_mode << 9) + (self.AUX_output_enable << 8) + (self.AUX_output_power << 6) + (self.RF_ENABLE << 5) + (self.RF_output_power << 3) + 4
        self.registers.reg[5] = (self.LD << 22) + (0x3 << 19) + 5
        #print([f"0x{val:08X}" for val in self.reg]) #print hex value
        pass

def list_usb_devices(vid,pid):
    devices = hid.enumerate()
    matching_devices = [dev for dev in devices if dev['vendor_id'] == vid and dev['product_id'] == pid]

    if matching_devices:
        print(f"Found {len(matching_devices)} device(s) with VID=0x{vid:04X} PID=0x{pid:04X}:\n")
        for dev in matching_devices:
            print(f"  Path       : {dev['path'].decode() if isinstance(dev['path'], bytes) else dev['path']}")
            print(f"  Manufacturer: {dev['manufacturer_string']}")
            print(f"  Product     : {dev['product_string']}")
            print(f"  Serial No.  : {dev['serial_number']}")
            print()
    else:
        print(f"No CircuitValley RFGEN devices found with VID=0x{vid:04X} PID=0x{pid:04X}")

def find_usb_device(vid,pid):
    devices = hid.enumerate()
    matching_devices = [dev for dev in devices if dev['vendor_id'] == vid and dev['product_id'] == pid]

    if matching_devices:
        return [len(matching_devices), matching_devices[0]['serial_number']]
    else:
        print(f"No CircuitValley RFGEN devices found with VID=0x{vid:04X} PID=0x{pid:04X}")
        return [0, ""]


def write_to_hid_device(vid, pid, serial_number, data):
    try:
        with hid.Device(vid=vid, pid=pid, serial=serial_number) as device:
            if isinstance(data, list):
                data = bytes(data)

            bytes_written = device.write(data)
            print(f"Wrote {bytes_written} bytes to HID device.")
            return bytes_written

    except hid.HIDException as e:
        print(f"Failed to communicate with device: {e}")
        return -1
        
        
def frequency_to_uint32(num: float) -> int:
    # Ensure non-negative
    if num < 0:
        raise ValueError("Frequency unsigned floats are allowed")

    # Scale float to integer (preserve 2 decimal places)
    result = int(num * 100)
    # Force into unsigned 32-bit range
    return result & 0xFFFFFFFF
    
def calc_and_write(frequency, serial):
   RFGEN = AD4351()
   RFGEN.frequency = frequency
   RFGEN.BuildRegisters()
   RFGEN.registers.frequency = frequency_to_uint32(frequency)
   RFGEN.registers.isStartOnBoot = 1

   result_bytes = [0x00, COMMAND_SET_REG] + list(RFGEN.registers.mem)

   write_to_hid_device(TARGET_VID, TARGET_PID, serial, result_bytes)

def rf_out_write(rf_out, serial_number):
   rfctrl_bytes = [0x00, COMMAND_RF_CTRL, rf_out]
   if isinstance(rfctrl_bytes, list):
       data_out = bytes(rfctrl_bytes)
   data_out = data_out[:64].ljust(64, b'\x00')
   write_to_hid_device(TARGET_VID, TARGET_PID, serial_number, data_out)

def deviceCtrl(serial_number, frequency, write_flash, identify, erase):
   RFGEN = AD4351()
   RFGEN.frequency = frequency
   RFGEN.BuildRegisters()
   RFGEN.registers.frequency = frequency_to_uint32(frequency)
   RFGEN.registers.isStartOnBoot = 1
   result_bytes = [0x00, COMMAND_DEVICE_CTRL, write_flash, identify, erase] + list(RFGEN.registers.mem)
   write_to_hid_device(TARGET_VID, TARGET_PID, serial_number, result_bytes)

def programSweep(serial_number, startfrequency, stopfrequency, stepfrequency, steptime):
   RFGEN = AD4351()
   RFGEN.frequency = 35.0
   RFGEN.BuildRegisters()
   RFGEN.registers.frequency = frequency_to_uint32(35.0)
   RFGEN.registers.start_freq = frequency_to_uint32(startfrequency)
   RFGEN.registers.stop_freq = frequency_to_uint32(stopfrequency)
   RFGEN.registers.step_freq = frequency_to_uint32(stepfrequency)
   RFGEN.registers.step_ms = steptime
   RFGEN.registers.ref_freq = frequency_to_uint32(RFGEN.REF_FREQ)
   RFGEN.registers.isSweepEnabled = 1
   RFGEN.registers.isStartOnBoot = 1
   result_bytes = [0x00, COMMAND_DEVICE_CTRL, 0x01, 0x00, 0x00] + list(RFGEN.registers.mem)
   write_to_hid_device(TARGET_VID, TARGET_PID, serial_number, result_bytes)


def read_device_rf_out_status(serial_number):
    try:
            device = hid.Device(vid=TARGET_VID, pid=TARGET_PID, serial=serial_number)
            data_out = [0x00, COMMAND_READ_RF_CTRL]

            if isinstance(data_out, list):
                data_out = bytes(data_out)

            data_out = data_out[:64].ljust(64, b'\x00') # first index is report ID while rest 64 bytes are payload making it 65bytes

            bytes_written = device.write(data_out)
            if bytes_written != 64:
                raise IOError(f"Only wrote {bytes_written} bytes, expected 64")

            # Read response (64 bytes)
            response = device.read(64, timeout=1000)
            if not response:
                raise TimeoutError("No response received from device")

            if response[1]:
                print("RF out Enabled")
            else:
                print("RF out Disabled")

            device.close()
    except hid.HIDException as e:
        print(f"Failed to communicate with device: {e}")
        device.close()
        return -1

def read_device_firmware_build(serial_number):
    try:
            device = hid.Device(vid=TARGET_VID, pid=TARGET_PID, serial=serial_number)
            #device.nonblocking(True)
            # Send write
            data_out = [0x00, COMMAND_GET_BUILD_INFO]

            if isinstance(data_out, list):
                data_out = bytes(data_out)

            data_out = data_out[:64].ljust(64, b'\x00') # first index is report ID while rest 64 bytes are payload making it 65bytes

            bytes_written = device.write(data_out)
            if bytes_written != 64:
                raise IOError(f"Only wrote {bytes_written} bytes, expected 64")
            # Read response (64 bytes)
            #device.nonblocking(False)
            response = device.read(64, timeout=1000)
            if not response:
                raise TimeoutError("No response received from device")

            firmware_version_major = response[1]
            firmware_version_minor = response[2]
            firmware_build_number  = (response[3] << 8) | response[4]

            print("Serial Number {} Firmware version {}.{}.{}".format(serial_number, firmware_version_major, firmware_version_minor, firmware_build_number))

            device.close()
    except hid.HIDException as e:
        print(f"Failed to communicate with device: {e}")
        device.close()
        return -1

def validateFrequency(frequency):
    # Ensure non-negative
    if frequency < 35.0:
        print("Frequency < 35.0 Mhz is invalid")
        sys.exit(1)

    if frequency > 4400.0:
        print("Frequency > 4400.0 Mhz is invalid")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="CVRF Control Application")
    parser.add_argument(
        "-f", "--frequency", type=float, help="Set frequency value (float) Mhz i.e 50.00 for 50Mhz, min 35.00Mhz max 4400.00Mhz"
    )
    parser.add_argument(
        "-l",
        "--list",
        action="store_true",
        help="List all Circuitvalley RFGEN USB HID Devices",
    )
    parser.add_argument(
        "-s", "--serial_number", type=str, help="Target Device Serial number"
    )
    parser.add_argument(
        "-w",
        "--write",
        action="store_true",
        help="Write Frequency and other parameters to Circuitvalley RFGEN USB HID Device",
    )
    parser.add_argument(
        "-i",
        "--info",
        action="store_true",
        help="Get firmware version from Circuitvalley RFGEN USB HID Device",
    )

    parser.add_argument(
        "-t",
        "--flashwrite",
        action="store_true",
        help="Write Flash Circuitvalley RFGEN USB HID Device Notified by Device LED Blink",
    )

    parser.add_argument(
        "-d",
        "--devindetify",
        action="store_true",
        help="Indentify Circuitvalley RFGEN USB HID Device by blinking Device LED",
    )

    parser.add_argument(
        "-e",
        "--erase",
        action="store_true",
        help="Erase Flash of Circuitvalley RFGEN USB HID Device Notified by Device LED Blink",
    )
    
    parser.add_argument(
        "-r",
        "--rfstate",
        nargs='?',
        const='no value specified',
        type=str,
        help="Get/Set RF Out status from Circuitvalley RFGEN USB HID Device {true or false, 1 or 0}",
    )

    parser.add_argument(
        "-p",
        "--programsweep",
        action="store_true",
        help="Program Sweep to enable device sweep need to specify start stop step frequency aswell as step time",
    )
    
    
    parser.add_argument(
        "-a",
        "--auxselect",
        type=int,
        help="Aux Pin selection value (integer), 0 for Sync out, 1 for Sync IN, 2 for External Reference ",
    )
    
    parser.add_argument(
        "-b",
        "--startfrequency",
        type=float,
        help="Start Frequency value (float) Mhz i.e 50.00 for 50Mhz , min 35.00Mhz max 4400.00Mhz",
    )
    
    parser.add_argument(
        "-c",
        "--stopfrequency",
        type=float,
        help="Stop Frequency value (float) Mhz i.e 50.00 for 50Mhz, min 0.01Mhz max 2200.00Mhz",
    )
    
    parser.add_argument(
        "-g",
        "--stepfrequency",
        type=float,
        help="Step Frequency value (float) Mhz i.e 50.00 for 50Mhz, min 35.00Mhz max 4400.00Mhz",
    )
    
    parser.add_argument(
        "-j",
        "--steptime",
        type=int,
        help="Time delay between steps value (integer) ms i.e 50 for 50ms",
    )
    
    
    args = parser.parse_args()
    
    if  args.serial_number is None and not args.list and  not len(sys.argv) == 1:
        number_of_devices, found_device_serial =  find_usb_device(TARGET_VID, TARGET_PID)
        if number_of_devices == 1 :
            print("RFGEN Device {}".format(found_device_serial))
            args.serial_number = found_device_serial
        elif number_of_devices > 1 :
            print("Error: Multiple USB devices found operations requires --serial to be specified.")
            sys.exit(1)
        else:
            sys.exit(1)
            
    if args.frequency is not None:
        print(f"Frequency set to: {args.frequency} MHz")

    if args.list:
        list_usb_devices(TARGET_VID, TARGET_PID)

    if args.write:
        if args.frequency is None:
            print("Error: --write operation requires --frequency to be specified.")
            sys.exit(1)
        
        validateFrequency(args.frequency)
        calc_and_write(args.frequency, args.serial_number)

    if args.info:
        read_device_firmware_build(args.serial_number)

    if args.flashwrite:
        if args.frequency is None:
            print("Error: --flashwrite operation requires --frequency to be specified.")
            sys.exit(1)
         
        validateFrequency(args.frequency)
        deviceCtrl(args.serial_number, args.frequency, 1, 0, 0)

    if args.devindetify:
        deviceCtrl(args.serial_number, 35.0, 0, 1, 0)
        
    if args.erase:
        deviceCtrl(args.serial_number, 35.0, 1, 1, 1)  #frequency default , Write flash , identify, erase

    if args.programsweep:
        if args.startfrequency is None or  args.stopfrequency is None or args.stepfrequency is None or args.steptime is None:
            print("Error: Sweep operation requires startfrequency, stopfrequency, stepfrequency, steptime to be specified.")
            sys.exit(1)
            
        if args.startfrequency > args.stopfrequency:
            print("Error: Sweep operation requires startfrequency to be less than stopfrequency.")
            sys.exit(1)

        if (args.startfrequency + args.stepfrequency) > args.stopfrequency:
            print("Error: Sweep operation Step too big.")
            sys.exit(1)

        validateFrequency(args.startfrequency)
        validateFrequency(args.stopfrequency)
        
        if  args.steptime < 30:
            print("Error: Sweep operation Step is too small set to 30ms.")

        programSweep(args.serial_number, args.startfrequency, args.stopfrequency, args.stepfrequency, args.steptime)

    if args.rfstate:
        value = args.rfstate.lower()
        if value == 'no value specified':
            read_device_rf_out_status(args.serial_number)
        elif value in ('true', '1'):
            rf_out_write(1, args.serial_number)
        elif value in ('false', '0'):
            rf_out_write(0, args.serial_number)
        else:
            print(f"Value: {args.arrangement}")
            

    if  len(sys.argv) == 1:
        parser.print_help()
        sys.exit(1)


if __name__ == "__main__":
    main()
