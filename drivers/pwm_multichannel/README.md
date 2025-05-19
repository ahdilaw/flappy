Supports upto 8 concurrent PWM output channels, on 8 unique GPIOs by adjusting the width, provided as argument to initialization.

## Usage
```bash
insmod pwm_mc.ko gpio_pins=535,536,537
```

## Note:
 - GPIO Pins must be in BCM numbering format.
 - Makes pwm driver modules as specified under `/dev/pwm*` in the kernel.