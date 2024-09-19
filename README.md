# HP5082-7300 Clock

`Image Placeholder`

This repository contains the PlatformIO project for the Wemos D1 Mini based clock using the HP5082-7300 7-segment display. The repository also has the necessary 3D model files for the case. Since my case was designed around the protoboard I soldered the segments onto, it might not be compatible with your setup. Thus the Fusion 360 files as well as step files are included for you to modify.

## Features
- NTP time synchronization
- Timezone support
- DST support
- Web interface for configuration
- OTA updates*
 
*OTA updates are handled though the WiFiManager library, which requires you to upload a firmware file though the captive portal. You will need to remove the dummy OTA callback that is currently in the firmware for "security" reasons.

## Build instructions
### Hardware
- 1x Wemos D1 Mini
- 4x HP5082-7300 7-segment display
- 1x Button of any kind
- 1x 2k-100k pull-up resistor for the button
- 1x Thin sheet of plastic to protect the displays
- 2x 3.5M captive screws
- Protoboard, wires, etc.
- 3D printed case

#### Pinout
##### Segment Control Pins

These are the pins that select what to display on the display, the HP5082-7300 has it's own binary decoder and memory, meaning that we can wire all of these in parallel for all the segments and control the latching with the enable pins.

| Pin Number | Pin Name | Description           |
|------------|----------|-----------------------|
| D1         | DIGIT_PIN_1 | Binary digit selection pin (LSB) |
| D2         | DIGIT_PIN_2 | Binary digit selection pin |
| D3         | DIGIT_PIN_4 | Binary digit selection pin |
| D4         | DIGIT_PIN_8 | Binary digit selection pin (MSB) |

##### Display Enable Pins

Each display in the 4x HP5082-7300 array is enabled individually using the following pins:

| Pin Number | Pin Name       | Description                        |
|------------|----------------|------------------------------------|
| D8         | DIGIT_0_EN_PIN | Enables the digit 0 |
| D7         | DIGIT_1_EN_PIN | Enables the digit 1 |
| D6         | DIGIT_2_EN_PIN | Enables the digit 2 |
| D5         | DIGIT_3_EN_PIN | Enables the digit 3 |

##### Dot Pin

The dot is a decimal point that is pulsed once a second, it connected to the digit that separates the hours and minutes. Connect this to `D0`.

##### Button Pin

The button is connected to `A0` and it pulled up to 3.3V externally using the resistor. The button shorts the pin to ground when pressed. The analog pin is used because there were not enough pins on the Wemos D1 Mini to use a separate pin for the button.

### Software
1. Clone/Download the repository
2. Open the HP50827300Clock in [PlatformIO](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide)
3. Change the `DST_REGION` to one of the following:
`DST_EUROPE`,
`DST_NORTH_AMERICA`,
`DST_AUSTRALIA`,
`DST_SOUTH_AMERICA`
4. Build and upload the firmware to the Wemos D1 Mini

## Usage
After the code is uploaded the clock will display `----` and create a WiFi network called `LED Clock Config`. You can use your phone to connect to this network and it should automatically open a configuration page.
- First, enter the `Setup` menu and configure your timezone and DST preference. You can leave the NTP server as is. Click `Save` and it should show a confirmation message.

If the page does not put you back to the main page you can manually go back until you reach it.

- Next enter the `Configure WiFi` menu (it will take a bit to open since it is scanning for networks). Click the network name you'd like to connect to and enter the password. Click `Save` and it should show a confirmation message.

Within a minute the clock should connect to the network and start displaying the time. If it doesn't the clock will keep displaying `----` and re-open the `LED Clock Config` access point for you to try configuring it again.

### Resetting

As mentioned before, when the clock can't connect to the WiFi network it will re-open the `LED Clock Config` access point. But if you want to reset it manually you can press and hold the button for 5 seconds. The clock will display an animation while you are holding the button and show `----` once it is reset, you can let go of the button at that point, and now use the access point to configure it again.