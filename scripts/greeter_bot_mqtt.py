import time
from PIL import Image, ImageDraw
import board, busio, adafruit_ssd1306
import os
import paho.mqtt.client as mqtt
import json
import threading
from threading import Lock

# MQTT Configuration
MQTT_BROKER = "localhost"
MQTT_PORT = 1883
TOPIC_EYES = "greeter/eyes"
TOPIC_SERVO = "greeter/servo"
TOPIC_EVENTS = "greeter/events"

# Customer Counts
customer_count = 0
count_lock = Lock()

# Device paths
PWM_DEVICES = ["/dev/pwm_mc0", "/dev/pwm_mc1", "/dev/pwm_mc2"]
IR_FRONT = "/dev/ir_mc0"   # close-range
IR_LONG  = "/dev/ir_mc1"   # far-range

# OLED Setup
WIDTH, HEIGHT = 128, 64
i2c = busio.I2C(board.SCL, board.SDA)
oledL = adafruit_ssd1306.SSD1306_I2C(WIDTH, HEIGHT, i2c, addr=0x3C)  # Left eye
oledR = adafruit_ssd1306.SSD1306_I2C(WIDTH, HEIGHT, i2c, addr=0x3D)  # Right eye

# Servo PWM Constants
PERIOD = 20000000
DUTY_0 = 1000000
DUTY_90 = 1500000
DUTY_180 = 2000000

# Add MQTT Client
client = mqtt.Client()
servo_positions = [0, 0, 0]  # Track positions for individual servo control

def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT Broker")
    client.subscribe([(TOPIC_EYES, 0), (TOPIC_SERVO, 0), (TOPIC_EVENTS, 0)])

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode()
        if msg.topic == TOPIC_EYES:
            draw_eyes(payload)
            
        elif msg.topic == TOPIC_SERVO:
            data = json.loads(payload)
            ch = data.get("channel")
            angle = data.get("angle")
            if 0 <= ch <= 2 and 0 <= angle <= 180:
                move_servo(ch, angle)
                servo_positions[ch] = angle
                
        elif msg.topic == TOPIC_EVENTS:
            handle_event(payload)
            
    except Exception as e:
        print("MQTT Error:", e)

def move_servo(channel, degree):
    duty = int(DUTY_0 + (degree * (DUTY_180 - DUTY_0)/180))
    write_pwm(channel, PERIOD, duty, 1)

def handle_event(event):
    if event == "reset":
        move_all_servos(0)
        draw_eyes("neutral")
    elif event == "dance":
        # Add custom animation sequence
        for _ in range(3):
            move_all_servos(90)
            draw_eyes("welcome")
            time.sleep(0.5)
            move_all_servos(0)
            draw_eyes("neutral")
            time.sleep(0.5)

def write_pwm(channel, period, duty, enable):
    with open(PWM_DEVICES[channel], "w") as f:
        f.write(f"{channel} {period} {duty} {enable}")

def setup_servos():
    for ch in range(3):
        write_pwm(ch, PERIOD, DUTY_0, 1)

def move_all_servos(degree):
    if degree == 0:
        duty = DUTY_0
    elif degree == 90:
        duty = DUTY_90
    elif degree == 180:
        duty = DUTY_180
    else:
        raise ValueError("Invalid degree")
    for ch in range(3):
        write_pwm(ch, PERIOD, duty, 1)

def read_ir(path):
    try:
        with open(path, "r") as f:
            return f.read(1) == '0'
    except:
        return False

# -------- OLED Eye Animations --------
def draw_eyes(expr="neutral"):
    imgL = Image.new("1", (WIDTH, HEIGHT))
    imgR = Image.new("1", (WIDTH, HEIGHT))
    dL = ImageDraw.Draw(imgL)
    dR = ImageDraw.Draw(imgR)

    if expr == "welcome":
        dL.ellipse((32, 16, 96, 64), outline=255, fill=0)
        dR.ellipse((32, 16, 96, 64), outline=255, fill=0)
    elif expr == "angry":
        dL.polygon([(20, 20), (100, 30), (100, 50), (20, 40)], fill=255)
        dR.polygon([(20, 30), (100, 20), (100, 40), (20, 50)], fill=255)
    elif expr == "sleep":
        dL.line((32, 32, 96, 32), fill=255, width=3)
        dR.line((32, 32, 96, 32), fill=255, width=3)
    else:
        dL.rectangle((48, 24, 80, 56), outline=255, fill=255)
        dR.rectangle((48, 24, 80, 56), outline=255, fill=255)

    oledL.image(imgL)
    oledR.image(imgR)
    oledL.show()
    oledR.show()

# -------- Main Logic --------
def main():
    setup_servos()
    draw_eyes("neutral")
    
    # MQTT Setup
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(MQTT_BROKER, MQTT_PORT)
    client.loop_start()

    print("?? Bot ready with MQTT! Ctrl+C to quit.")

    last_greet = 0
    cooldown = 5  # seconds

    try:
        while True:
            now = time.time()
            detected_far  = read_ir(IR_LONG)
            detected_near = read_ir(IR_FRONT)

            if detected_far and not detected_near and (now - last_greet > cooldown):
                with count_lock:
                    global customer_count
                    customer_count += 1
                    client.publish("greeter/count", str(customer_count))  # Add this line
                print(f"?? Welcome detected - Total customers: {customer_count}")
                draw_eyes("welcome")
                move_all_servos(90)
                time.sleep(1)
                move_all_servos(0)
                last_greet = now

            elif detected_far and detected_near:
                print("?? Too close! Back off!")
                draw_eyes("angry")
                move_all_servos(180)
                time.sleep(0.5)
                move_all_servos(0)
                time.sleep(0.5)

            else:
                draw_eyes("neutral")
                move_all_servos(0)

            time.sleep(0.2)

    except KeyboardInterrupt:
        print("Shutting down...")
        draw_eyes("sleep")
        client.loop_stop()
        for ch in range(3):
            write_pwm(ch, 0, 0, 0)

if __name__ == "__main__":
    main()