# Zephyr-based BTHome device

![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)

I've been building various microcontroller projects to work with
[Home Assistant](https://home-assistant.io) to automate various tasks.
[ESPHome](https://esphome.io) has been my framework of choice, because it
integrates well with Home Assistant, and is very easy to use.  As I have
progressed in the automation of various tasks, I find myself in need of more
battery-operated projects, and this is where using WiFi begins to fall down.
I found myself wanting to be able to send events and data over BLE to an
ESPHome Bluetooth Proxy board; this led me to discovering [BTHome](https://bthome.io).
There wasn't a lot of detailed description of how to implement sensors (or
button-based "remotes", like I wanted), so I decided I should create my own
project to document things for myself - that's what this project attempts to be.

BTHome-Zephyr is a project based on the [Zephyr RTOS](https://github.com/zephyrproject-rtos/).
It currently builds against version 3.5; it uses the input subsytem introduced with
Zephyr 3.4, so may well work with that release, though it hasn't been tested.

One of the reasons for using Zephyr is its rich support of various microcontrollers
and boards; the project has been tested with the Nordic nRF52840 Development Kit,
the Lolin C3 Pico ESP32C3 board (as "esp32c3_devkitm"), the STM32 Nucleo WB55RG,
and the Lolin32 Lite (as "esp32_devkitc_wroom").

## Features

- Send BTHome button events for up to four physical over Bluetooth Low Energy (BLE)
  either to Home Assistant directly, or via an ESPHome Bluetooth Proxy board.
- Supports "press" and "long press" events for four buttons.
- Easily build for any Zephyr-supported board.
- Simple and hopefully well-documented code.

## Requirements

- A [Zephyr development environment](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)

## Getting Started

Follow these steps to get started with BTHome Zephyr:

1. Clone the repository:

```bash
git clone https://github.com/jeffrizzo/bthome-zephyr.git
```

2. Navigate to the project directory, and intialize the project using `west`:

```bash
cd bthome-zephyr
west init -l app
west update
```

This will clone the correct version of Zephyr and set up the `west` environment.

3. Build the project for your target board. For example, to build for the nRF52840
development kit:

```bash
west build -b nrf52840dk_nrf52840 app
```

4. Flash the built image onto your hardware board using `west flash`


## Usage

This is, of course, intended to be used with the [Home Assistant BTHome integration]
(https://www.home-assistant.io/integrations/bthome/). As long as your Home Assistant
instance has the [Bluetooth](https://www.home-assistant.io/integrations/bluetooth)
integration enabled, or you're using an [ESPHome Bluetooth Proxy]
(https://esphome.github.io/bluetooth-proxies/), your device should just appear on
the "Integrations" page of your Home Assistant install.  When opening the device page
for the new device, you'll see button events appear in the "Logbook" section.

Using these events is a little trickier; they appear as Home Assistant events, and
you'll have to use them as such in HA.  One way to get more information is to
navigate to the "Developer Tools"->"Events" section of your HA install. Under
"Listen to events", put `bthome_ble_event` under "Event to subscribe to", and click
"Start Listening". You should see an event fire for every button press, and they'll
look something like this:

```
event_type: bthome_ble_event
data:
  device_id: 7d191a94d05496538c79a63752f125d0
  address: CA:E1:AF:67:EE:EF
  event_class: button_4
  event_type: long_press
  event_properties: null
origin: LOCAL
time_fired: "2024-02-11T20:20:26.975235+00:00"
context:
  id: 01HPCX82PZV9MVQY2F36KH7T93
```

... you'll need to match on the `device_id`, `event_class` and `event_type` fields
in your automations.

One of the other issues I'd like to solve is the
current lack of simple "here's how to see if it's working" instructions.  One way
is you can use a tool like Nordic's
[nRFConnect](https://www.nordicsemi.com/Products/Development-tools/nrf-connect-for-mobile)
mobile app to verify that button presses cause advertisements to be sent, and
that they change accordingly, as a quick and dirty "am I sending packets" check.

## Contributing

Contributions are welcome! If you'd like to contribute to this project, please submit
a Pull Request:

- Fork the repository.
- Create a new branch (`git checkout -b feature/my_feature`).
- Make your changes.
- Commit your changes (`git commit -am 'Add new feature'`).
- Push to the branch (`git push origin feature/my_feature`).
- Create a new Pull Request.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgements

- Initial support and investigations loosely modeled on the
  [`bthome_sensor_template`](https://github.com/zephyrproject-rtos/zephyr/tree/main/samples/bluetooth/bthome_sensor_template)
  sample Zephyr application.
