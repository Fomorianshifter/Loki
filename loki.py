from ctypes import CDLL, c_uint8, c_uint16, c_int

loki = CDLL("./loki_core.so")

# Define argument/return types
loki.loki_eeprom_read.argtypes = [c_uint8, c_uint8 * 256, c_uint16]
loki.loki_eeprom_read.restype = c_int

loki.loki_eeprom_write.argtypes = [c_uint8, c_uint8 * 256, c_uint16]
loki.loki_eeprom_write.restype = c_int

def eeprom_read(address: int, length: int = 16) -> bytes:
    buf = (c_uint8 * 256)()
    status = loki.loki_eeprom_read(address, buf, length)
    if status != 0:
        raise RuntimeError(f"EEPROM read failed: {status}")
    return bytes(buf[:length])

def eeprom_write(address: int, data: bytes) -> None:
    buf = (c_uint8 * 256)()
    for i, b in enumerate(data):
        buf[i] = b
    status = loki.loki_eeprom_write(address, buf, len(data))
    if status != 0:
        raise RuntimeError(f"EEPROM write failed: {status}")
