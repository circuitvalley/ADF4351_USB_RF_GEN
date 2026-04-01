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
COMMAND_SET_MACRO = 0x86
COMMAND_GET_MACRO = 0x87

# Macro step flag definitions (matches firmware system.h)
STEP_FLAG_ENABLED    = 0x01
STEP_FLAG_RF_MASK    = 0x06
STEP_FLAG_RF_SHIFT   = 1
STEP_FLAG_RF_ON      = 0x01
STEP_FLAG_RF_OFF     = 0x02
STEP_FLAG_RECALC_PLL = 0x08

MACRO_MAX_STEPS = 14
MACRO_STEPS_PER_BLOCK = 7


class ADF4351_reg_t(ctypes.Union):
    class _Regs(ctypes.Structure):
        _fields_ = [
            ("reg", ctypes.c_uint32 * 6),         # 24 bytes
            ("frequency", ctypes.c_uint32),        # 4 bytes
            ("ref_freq", ctypes.c_uint32),         # 4 bytes
            ("start_freq", ctypes.c_uint32),       # 4 bytes
            ("stop_freq", ctypes.c_uint32),        # 4 bytes
            ("step_freq", ctypes.c_uint32),        # 4 bytes
            ("step_ms", ctypes.c_uint16),          # 2 bytes
            ("aux_select", ctypes.c_uint16),       # 2 bytes
            ("isSweepEnabled", ctypes.c_uint8),    # 1 byte
            ("isStartOnBoot", ctypes.c_uint8),     # 1 byte
            ("isStartOfSweep", ctypes.c_uint8),    # 1 byte
            ("flashWritePending", ctypes.c_uint8), # 1 byte
            ("flashReadPending", ctypes.c_uint8),  # 1 byte
            ("isMacroEnabled", ctypes.c_uint8),    # 1 byte
            ("sanityCheck", ctypes.c_uint16),      # 2 bytes
        ]

    _anonymous_ = ("regs",)
    _fields_ = [
        ("regs", _Regs),
        ("mem", ctypes.c_uint8 * 56)
    ]


def uint24_to_bytes(val):
    """Convert integer to 3-byte little-endian (XC8 uint24_t)."""
    val = int(val) & 0xFFFFFF
    return bytes([val & 0xFF, (val >> 8) & 0xFF, (val >> 16) & 0xFF])


def pack_step(frequency_fixed, step_ms, status_flag):
    """Pack a single stepConfig_t: uint24 freq + uint24 ms + uint8 flag = 7 bytes."""
    return uint24_to_bytes(frequency_fixed) + uint24_to_bytes(step_ms) + bytes([status_flag])


def pack_macro_block(steps):
    """Pack up to 7 steps into a devicemacro_t block (56 bytes).
    steps: list of (frequency_fixed, step_ms, status_flag) tuples.
    """
    data = bytearray()
    for freq, ms, flag in steps:
        data += pack_step(freq, ms, flag)

    # Pad unused steps to zero (7 bytes each)
    for _ in range(MACRO_STEPS_PER_BLOCK - len(steps)):
        data += b'\x00' * 7

    # macroSteps count (1 byte)
    data += bytes([len(steps)])

    # Padding to reach 56 bytes
    while len(data) < 56:
        data += b'\x00'

    return bytes(data[:56])


class AD4351:
    def __init__(self):
        # Public members
        self.REF_FREQ = 25.0
        self.ref_doubler = False
        self.ref_div2 = False
        self.enable_gcd = True
        self.feedback_select = True
        self.band_select_clock_mode = False
        self.clock_divider = 150
        self.band_select_clock_freq = 0.0
        self.band_select_auto = True
        self.N = 0.0
        self.PFDFreq = 0.0
        self.PHASE_ADJUST = False
        self.PR1 = True
        self.low_noise_spur_mode = 0
        self.muxout = 0
        self.charge_pump_current = 7
        self.LDF = False
        self.LDP = False
        self.PD_Polarity = True
        self.power_down = False
        self.cp_3stage = False
        self.counter_reset = False
        self.double_buff = False
        self.ABP = False
        self.charge_cancelletion = False
        self.CSR = False
        self.CLK_DIV_MODE = 0
        self.LD = 1
        self.VCO_power_down = False
        self.MTLD = False
        self.AUX_output_mode = False
        self.AUX_output_enable = False
        self.AUX_output_power = 0
        self.RF_output_power = 3
        self.RF_ENABLE = True
        self.PHASE = 1
        self.r_counter = 50
        self.frequency = 100.0
        self.INT = 0
        self.MOD = 0.0
        self.FRAC = 0.0
        self.band_select_clock_divider = 0
        self.registers = ADF4351_reg_t()

    @staticmethod
    def gcd(a: int, b: int) -> int:
        a &= 0xFFFFFFFF
        b &= 0xFFFFFFFF
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

        self.registers.reg[0] = ((self.INT & 0xFFFF) << 15) + ((self.FRAC & 0xFFF) << 3) + 0
        self.registers.reg[1] = (self.PHASE_ADJUST << 28) + (self.PR1 << 27) + (self.PHASE << 15) + ((self.MOD & 0xFFF) << 3) + 1
        self.registers.reg[2] = (self.low_noise_spur_mode << 29) + (self.muxout << 26) + ((1 if self.ref_doubler else 0) << 25) + ((1 if self.ref_div2 else 0) << 24) + (self.r_counter << 14) + (self.double_buff << 13) + (self.charge_pump_current << 9) + (self.LDF << 8) + (self.LDP << 7) + (self.PD_Polarity << 6) + (self.power_down << 5) + (self.cp_3stage << 4) + (self.counter_reset << 3) + 2
        self.registers.reg[3] = (self.band_select_clock_mode << 23) + (self.ABP << 22) + (self.charge_cancelletion << 21) + (self.CSR << 18) + (self.CLK_DIV_MODE << 15) + (self.clock_divider << 3) + 3
        self.registers.reg[4] = (self.feedback_select << 23) + (int(math.log2(output_divider)) << 20) + (self.band_select_clock_divider << 12) + (self.VCO_power_down << 11) + (self.MTLD << 10) + (self.AUX_output_mode << 9) + (self.AUX_output_enable << 8) + (self.AUX_output_power << 6) + (self.RF_ENABLE << 5) + (self.RF_output_power << 3) + 4
        self.registers.reg[5] = (self.LD << 22) + (0x3 << 19) + 5


def list_usb_devices(vid, pid):
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


def find_usb_device(vid, pid):
    devices = hid.enumerate()
    matching_devices = [dev for dev in devices if dev['vendor_id'] == vid and dev['product_id'] == pid]

    if matching_devices:
        return [len(matching_devices), matching_devices[0]['serial_number']]
    else:
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
    if num < 0:
        raise ValueError("Frequency must be non-negative")
    result = int(num * 100)
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


def programMacro(serial_number, steps):
    """Program macro steps to device flash.

    steps: list of dicts with keys:
        'frequency' (float MHz), 'duration' (int ms), 'rf' (bool)
    """
    if len(steps) < 2:
        print("Error: Macro requires at least 2 steps.")
        sys.exit(1)
    if len(steps) > MACRO_MAX_STEPS:
        print(f"Error: Macro supports maximum {MACRO_MAX_STEPS} steps.")
        sys.exit(1)

    # Convert steps to firmware format
    packed_steps = []
    prev_freq = None
    for i, step in enumerate(steps):
        freq_fixed = frequency_to_uint32(step['frequency'])  # *100 fixed point
        duration_ms = step['duration']

        # Status flag
        flag = STEP_FLAG_ENABLED
        flag |= (STEP_FLAG_RF_ON if step['rf'] else STEP_FLAG_RF_OFF) << STEP_FLAG_RF_SHIFT

        # PLL recalc: always on first step, or when frequency changes
        if prev_freq is None or freq_fixed != prev_freq:
            flag |= STEP_FLAG_RECALC_PLL
        prev_freq = freq_fixed

        packed_steps.append((freq_fixed, duration_ms, flag))

    # Split into two blocks of 7
    block0_steps = packed_steps[:MACRO_STEPS_PER_BLOCK]
    block1_steps = packed_steps[MACRO_STEPS_PER_BLOCK:]

    block0_data = pack_macro_block(block0_steps)
    block1_data = pack_macro_block(block1_steps)

    # Send block 0
    pkt0 = bytes([0x00, COMMAND_SET_MACRO, 0x00]) + block0_data
    pkt0 = pkt0[:64].ljust(64, b'\x00')
    print(f"Writing macro block 0 ({len(block0_steps)} steps)...")
    write_to_hid_device(TARGET_VID, TARGET_PID, serial_number, pkt0)

    # Send block 1
    pkt1 = bytes([0x00, COMMAND_SET_MACRO, 0x01]) + block1_data
    pkt1 = pkt1[:64].ljust(64, b'\x00')
    print(f"Writing macro block 1 ({len(block1_steps)} steps)...")
    write_to_hid_device(TARGET_VID, TARGET_PID, serial_number, pkt1)

    # Enable macro mode via COMMAND_DEVICE_CTRL with isMacroEnabled=1
    RFGEN = AD4351()
    RFGEN.frequency = steps[0]['frequency']
    RFGEN.BuildRegisters()
    RFGEN.registers.frequency = frequency_to_uint32(steps[0]['frequency'])
    RFGEN.registers.ref_freq = frequency_to_uint32(RFGEN.REF_FREQ)
    RFGEN.registers.isMacroEnabled = 1
    RFGEN.registers.isSweepEnabled = 0
    RFGEN.registers.isStartOnBoot = 1
    result_bytes = [0x00, COMMAND_DEVICE_CTRL, 0x01, 0x00, 0x00] + list(RFGEN.registers.mem)
    print("Enabling macro mode and writing settings to flash...")
    write_to_hid_device(TARGET_VID, TARGET_PID, serial_number, result_bytes)

    print(f"\nMacro programmed: {len(steps)} steps")
    for i, step in enumerate(steps):
        rf_str = "ON" if step['rf'] else "OFF"
        pll_str = "PLL" if (packed_steps[i][2] & STEP_FLAG_RECALC_PLL) else "   "
        print(f"  Step {i+1:2d}: {step['frequency']:10.2f} MHz  {step['duration']:6d} ms  RF:{rf_str}  {pll_str}")


def read_device_rf_out_status(serial_number):
    try:
        device = hid.Device(vid=TARGET_VID, pid=TARGET_PID, serial=serial_number)
        data_out = [0x00, COMMAND_READ_RF_CTRL]

        if isinstance(data_out, list):
            data_out = bytes(data_out)

        data_out = data_out[:64].ljust(64, b'\x00')

        bytes_written = device.write(data_out)
        if bytes_written != 64:
            raise IOError(f"Only wrote {bytes_written} bytes, expected 64")

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


def read_device_firmware_build(serial_number, silent=False):
    """Read firmware version. Returns (major, minor, build) or None on failure."""
    try:
        device = hid.Device(vid=TARGET_VID, pid=TARGET_PID, serial=serial_number)
        data_out = bytes([0x00, COMMAND_GET_BUILD_INFO])
        data_out = data_out[:64].ljust(64, b'\x00')

        bytes_written = device.write(data_out)
        if bytes_written != 64:
            raise IOError(f"Only wrote {bytes_written} bytes, expected 64")

        response = device.read(64, timeout=1000)
        if not response:
            raise TimeoutError("No response received from device")

        major = response[1]
        minor = response[2]
        build = (response[3] << 8) | response[4]

        if not silent:
            print("Serial Number {} Firmware version {}.{}.{}".format(serial_number, major, minor, build))

        device.close()
        return (major, minor, build)
    except hid.HIDException as e:
        print(f"Failed to communicate with device: {e}")
        try:
            device.close()
        except:
            pass
        return None


def validateFrequency(frequency):
    if frequency < 35.0:
        print("Frequency < 35.0 Mhz is invalid")
        sys.exit(1)
    if frequency > 4400.0:
        print("Frequency > 4400.0 Mhz is invalid")
        sys.exit(1)


def parse_macro_step(step_str):
    """Parse a macro step string: 'frequency_mhz,duration_ms,on/off'
    Examples: '100.0,500,on'  '2400.5,1000,off'
    """
    parts = step_str.strip().split(',')
    if len(parts) != 3:
        print(f"Error: Invalid macro step format '{step_str}'. Expected: frequency_mhz,duration_ms,on|off")
        sys.exit(1)

    try:
        freq = float(parts[0])
    except ValueError:
        print(f"Error: Invalid frequency '{parts[0]}' in macro step.")
        sys.exit(1)

    try:
        duration = int(parts[1])
    except ValueError:
        print(f"Error: Invalid duration '{parts[1]}' in macro step.")
        sys.exit(1)

    rf_str = parts[2].strip().lower()
    if rf_str in ('on', '1', 'true'):
        rf = True
    elif rf_str in ('off', '0', 'false'):
        rf = False
    else:
        print(f"Error: Invalid RF state '{parts[2]}' in macro step. Use on/off/1/0.")
        sys.exit(1)

    validateFrequency(freq)
    if duration < 30:
        print(f"Error: Step duration {duration}ms is too short (minimum 30ms).")
        sys.exit(1)

    return {'frequency': freq, 'duration': duration, 'rf': rf}


def main():
    parser = argparse.ArgumentParser(
        description="CircuitValley RFGEN44 RF Signal Generator Control Application Version 1.1",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  Program device sweep (100-200 MHz, 1 MHz steps, 50ms per step):
    %(prog)s --programsweep 100.0,200.0,1.0,50

  Program 3-step macro:
    %(prog)s --programmacro --macrostep 100.0,500,on --macrostep 200.0,500,on --macrostep 100.0,1000,off

  Each --macrostep takes: frequency_mhz,duration_ms,rf_state
    frequency_mhz : 35.0 - 4400.0
    duration_ms   : 30+
    rf_state      : on/off (or 1/0)
""")
    parser.add_argument(
        "-f", "--frequency", type=float,
        help="Set frequency value (float) Mhz i.e 50.00 for 50Mhz, min 35.00Mhz max 4400.00Mhz"
    )
    parser.add_argument(
        "-l", "--list", action="store_true",
        help="List all Circuitvalley RFGEN USB HID Devices",
    )
    parser.add_argument(
        "-s", "--serial_number", type=str,
        help="Target Device Serial number"
    )
    parser.add_argument(
        "-w", "--write", action="store_true",
        help="Write Frequency and other parameters to Circuitvalley RFGEN USB HID Device",
    )
    parser.add_argument(
        "-i", "--info", action="store_true",
        help="Get firmware version from Circuitvalley RFGEN USB HID Device",
    )
    parser.add_argument(
        "-t", "--flashwrite", action="store_true",
        help="Write Flash Circuitvalley RFGEN USB HID Device Notified by Device LED Blink",
    )
    parser.add_argument(
        "-d", "--devindetify", action="store_true",
        help="Identify Circuitvalley RFGEN USB HID Device by blinking Device LED",
    )
    parser.add_argument(
        "-e", "--erase", action="store_true",
        help="Erase Flash of Circuitvalley RFGEN USB HID Device Notified by Device LED Blink",
    )
    parser.add_argument(
        "-r", "--rfstate", nargs='?', const='no value specified', type=str,
        help="Get/Set RF Out status from Circuitvalley RFGEN USB HID Device {true or false, 1 or 0}",
    )
    parser.add_argument(
        "-p", "--programsweep", metavar="START,STOP,STEP,MS",
        help="Program device sweep: start_mhz,stop_mhz,step_mhz,step_ms  e.g. 100.0,200.0,1.0,50",
    )

    # Macro arguments
    parser.add_argument(
        "-m", "--programmacro", action="store_true",
        help="Program macro sequence to device flash (requires firmware 2.0+, min 2 steps, max 14 steps)",
    )
    parser.add_argument(
        "--macrostep", action="append", metavar="FREQ,MS,RF",
        help="Define a macro step: frequency_mhz,duration_ms,on|off  (repeat for each step, 2-14 steps)",
    )

    args = parser.parse_args()

    # Commands that require a connected device
    needs_device = (args.write or args.info or args.flashwrite or args.devindetify
                    or args.erase or args.rfstate or args.programsweep or args.programmacro)

    if args.serial_number is None and needs_device:
        number_of_devices, found_device_serial = find_usb_device(TARGET_VID, TARGET_PID)
        if number_of_devices == 1:
            print("RFGEN Device {}".format(found_device_serial))
            args.serial_number = found_device_serial
        elif number_of_devices > 1:
            print("Error: Multiple USB devices found, --serial_number must be specified.")
            sys.exit(1)
        else:
            print("Error: No RFGEN device connected.")
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
        deviceCtrl(args.serial_number, 35.0, 1, 1, 1)

    if args.programsweep:
        parts = args.programsweep.strip().split(',')
        if len(parts) != 4:
            print("Error: --programsweep format: start_mhz,stop_mhz,step_mhz,step_ms")
            sys.exit(1)

        try:
            startfrequency = float(parts[0])
            stopfrequency  = float(parts[1])
            stepfrequency  = float(parts[2])
            steptime       = int(parts[3])
        except ValueError:
            print("Error: --programsweep values must be: float,float,float,int")
            sys.exit(1)

        if startfrequency > stopfrequency:
            print("Error: Start frequency must be less than stop frequency.")
            sys.exit(1)

        if (startfrequency + stepfrequency) > stopfrequency:
            print("Error: Step frequency too large.")
            sys.exit(1)

        validateFrequency(startfrequency)
        validateFrequency(stopfrequency)

        if steptime < 30:
            print("Error: Step time too small (minimum 30ms).")
            sys.exit(1)

        programSweep(args.serial_number, startfrequency, stopfrequency, stepfrequency, steptime)

    if args.programmacro:
        if not args.macrostep or len(args.macrostep) < 2:
            print("Error: --programmacro requires at least 2 --macrostep arguments.")
            sys.exit(1)
        if len(args.macrostep) > MACRO_MAX_STEPS:
            print(f"Error: Maximum {MACRO_MAX_STEPS} macro steps allowed.")
            sys.exit(1)

        # Check firmware version — macro requires 2.0+
        fw = read_device_firmware_build(args.serial_number, silent=True)
        if fw is None:
            print("Error: Could not read firmware version from device.")
            sys.exit(1)
        if fw[0] < 2:
            print(f"Error: Device firmware {fw[0]}.{fw[1]}.{fw[2]} does not support macro programming.")
            print("       Firmware 2.0+ required. Please update firmware.")
            sys.exit(1)
        print(f"Device firmware {fw[0]}.{fw[1]}.{fw[2]} — macro supported.")

        steps = [parse_macro_step(s) for s in args.macrostep]
        programMacro(args.serial_number, steps)

    if args.rfstate:
        value = args.rfstate.lower()
        if value == 'no value specified':
            read_device_rf_out_status(args.serial_number)
        elif value in ('true', '1'):
            rf_out_write(1, args.serial_number)
        elif value in ('false', '0'):
            rf_out_write(0, args.serial_number)
        else:
            print(f"Value: {args.rfstate}")

    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(1)


if __name__ == "__main__":
    main()
