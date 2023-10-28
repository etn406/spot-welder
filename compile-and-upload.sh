#!/bin/sh
set -e

arduino-cli compile \
    --clean \
    --fqbn atmel-avr-xminis:avr:atmega328p_xplained_mini \
    --warnings default \
    --upload \
    --port "/dev/cu.usbserial-140" \
    --verify