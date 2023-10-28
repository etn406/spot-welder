# Spot Welder

Sorry this repo not properly organized, I had difficulties with separating C++ files, so everything ended up in one file.

## NewEncoder

I spent time searching for a library handling correctly rotary encoders (without skips/jumps)
and finally found [NewEncoder](https://github.com/gfvalvo/NewEncoder),
which I copied directly in the project root folder because I couldn't find a better way to include it.

## Components I used

- A module with an simple ESP8266 (no need for wifi/bluetooth)

- Monochrome OLED display 128x32 (OLED-091)

- Encoder with CLK/DT/SW data pins

- Momentary push button

- 5V transformer to power for the electronics (I used an old 500mA USB charger)

- Solid State Relay SSR-40DA

- Microwave transformer (opening a microwave, _even unplugged_, can **kill you** : please inquire about that)

- 25mm² copper wire (around 2 meters should be enough)

- copper connectors and electrodes, which can be made in a variety of forms and shapes.

- a power cord, with earth if possible (to connect it at least to the transformer's body.

## Todo

Schematics


