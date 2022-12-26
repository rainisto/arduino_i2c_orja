FROM debian:buster-slim
RUN apt-get update && apt-get install -y vim curl
RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
RUN cd bin && ./arduino-cli core update-index && ./arduino-cli core install arduino:avr && ./arduino-cli core install arduino:megaavr
COPY i2c_rtc_simplified_with_watchdog i2c_rtc_simplified_with_watchdog
COPY libraries libraries
RUN arduino-cli compile -e -b arduino:avr:pro:cpu=16MHzatmega328 --libraries /libraries i2c_rtc_simplified_with_watchdog
RUN arduino-cli compile -e -b arduino:avr:nano:cpu=atmega328 --warnings more --build-property build.extra_flags=-DMCP --libraries /libraries i2c_rtc_simplified_with_watchdog
RUN mkdir -p /output
RUN cp /i2c_rtc_simplified_with_watchdog/build/arduino.avr.pro/i2c_rtc_simplified_with_watchdog.ino.hex /output/i2c_rtc_simplified_with_watchdog_pro.ino.hex
RUN cp i2c_rtc_simplified_with_watchdog/build/arduino.avr.nano/i2c_rtc_simplified_with_watchdog.ino.hex /output/i2c_rtc_simplified_with_watchdog_nano.ino.hex

