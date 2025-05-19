# flappy
A Flappy Bird Customer Greeter Robot Implementation in submission for the project of EE 523 Embedded Systems.

## How to use?

```bash
insmod ir_mc.ko gpio_pins=529,538
insmod pwm_mc.ko gpio_pins=535,536,537
mosquitto
python3 greeter_bot_mqtt.py

(now in 1 terminal)
su - defense
node-red

(adresses):
http://192.168.137.135:1880/
http://192.168.137.135:1880/ui

(Wifi Specs auto-connect):
@ahdilaw
pwd: <pwd_hidden>
```
