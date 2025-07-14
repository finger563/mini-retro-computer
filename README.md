# Mini Retro Computer (MRP)

<img src="./images/icon.png" alt="MRP" width="300">

This repository contains code that allows an ESP + Display + Housing to act like
a little retro computer.

![image](https://github.com/user-attachments/assets/ff6183e9-9d7b-436b-8181-876472eb6c4a)

https://github.com/user-attachments/assets/a31ec884-594a-451a-a3d0-bb570fb42967

<!-- markdown-toc start - Don't edit this section. Run M-x markdown-toc-refresh-toc -->
**Table of Contents**

- [Mini Retro Computer (MRP)](#mini-retro-computer-mrp)
  - [Configure](#configure)
  - [Build and Flash](#build-and-flash)
  - [Output](#output)

<!-- markdown-toc end -->

## Configure

This project can run on either a `Byte90` or a `Waveshare ESP32-S3 TouchLCD`
board.

You can configure which board to use by running `idf.py menuconfig`.

## Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(Replace PORT with the name of the serial port to use.)

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Output

Example screenshot of the console output from this app:

![CleanShot 2025-06-10 at 12 57 32](https://github.com/user-attachments/assets/d79a88a1-72d9-48f3-ae98-42bd74bea8ea)

Waveshare ESP32-S3 TouchLCD board:

https://github.com/user-attachments/assets/19fd89e8-21b9-4e14-b6dd-c13d6cd386b7

<img width="715" height="949" alt="image" src="https://github.com/user-attachments/assets/d613c93b-9c47-4b58-a666-41708a364d97" />

Byte90:

https://github.com/user-attachments/assets/bb0ec5d7-b37c-44e3-a923-53e971ec151f

Older images:

![image](https://github.com/user-attachments/assets/4ddfd1dc-ff67-4175-80cf-85f58c5100f8)

https://github.com/user-attachments/assets/b5ee313d-070f-4589-8a12-f9fa04875219
