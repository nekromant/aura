BUILD_DIR=./build/

$(BUILD_DIR)packetizer_serial_test.out:$(BUILD_DIR)packetizer_serial_test.o  $(BUILD_DIR)packetizer_serial.o
	gcc -o $@ $^

$(BUILD_DIR)packetizer_serial_test.o:packetizer_serial_test.c
	gcc -c -o $@ $^

$(BUILD_DIR)packetizer_serial.o:packetizer_serial.c *.h 
	gcc -Wall -c -o $@ $< -I .

.PHONY: clear
clear:
	rm -r $(BUILD_DIR)*.o $(BUILD_DIR)*.out
