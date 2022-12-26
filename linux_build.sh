#!/bin/bash
docker build -t arduino_i2c .
docker run -v $(pwd)/hex:/hex -i arduino_i2c /bin/bash -c "cp /output/* /hex"
