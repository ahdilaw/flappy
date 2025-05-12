import time
from luma.core.interface.serial import i2c
from luma.oled.device import ssd1322

# OLED Setup
OLED_I2C_ADDRESS = 0x3C

def write_servo(angle):
    with open("/dev/mg90s_servo", "w") as f:
        f.write(str(angle))

def read_ir():
    with open("/dev/ir_sensor", "r") as f:
        return f.read(1)

class OLEDDisplay:
    def __init__(self):
        serial = i2c(port=1, address=OLED_I2C_ADDRESS)
        self.device = ssd1322(serial)
        self.device.clear()
        
    def show_message(self, msg):
        with canvas(self.device) as draw:
            draw.text((10, 20), msg, fill="white")
    
    def clear(self):
        self.device.clear()

def main():
    oled = OLEDDisplay()
    
    try:
        while True:
            ir_val = read_ir()
            
            if ir_val == '0':  # Customer detected
                write_servo(90)
                oled.show_message("Welcome!\n:)")
                time.sleep(0.5)
                write_servo(0)  # Flapping motion
            else:
                write_servo(0)
                oled.clear()
                
            time.sleep(0.1)
            
    except KeyboardInterrupt:
        oled.clear()
        print("System shutdown")

if __name__ == "__main__":
    main()