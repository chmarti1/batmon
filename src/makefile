# Installation target locations
# Edit these options prior to installation to change options
# Do not leave trailing / separators
SYSTEMD=/etc/systemd/system
HOME=/var/local/batmon
CONF=/etc/batmon

# Build dependent parameters
BIN=/usr/local/bin/batmon
BMSERVICE=$(SYSTEMD)/batmon.service
USER=batmon

# Build some dependency lists
LINK=-lm -llabjackusb


batmon.bin: batmon.c chargemodel.o autocom.o
	@echo "==> Compiling the batery monitor service binary"
	gcc -Wall -o batmon.bin autocom.o chargemodel.o batmon.c -lm -llabjackusb

chargemodel.o: chargemodel.c chargemodel.h
	@echo "==> Compiling the chargemodel object"
	gcc -Wall -c chargemodel.c -o chargemodel.o

autocom.o: autocom.c autocom.h
	@echo "==> Compiling the autocom binary"
	gcc -Wall -c autocom.c -o autocom.o

clean:
	rm -f *.bin
	rm -f *.o

install: batmon.bin
	@echo "==> Installing in the system..."
	@echo "==> Creating the batmon user and home dir"
	# Create the bmcmd user and home directory
	adduser --system --group --home $(HOME) $(USER)
	chmod 775 $(HOME)

	@echo "==> Placing the files..."
	# Install the bmcmd binary
	cp -f batmon.bin $(BIN)
	chown root:root $(BIN)
	chmod 775 $(BIN)

	# Install the configuration file
	@echo "==> Placing configuration files"
	mkdir $(CONF)
	cp -f *.conf $(CONF)
	chown -R root:batmon $(CONF)

	# Place the service unit file and start the service
	cp -f batmon.service $(HOME)
	ln -s $(HOME)/batmon.service $(BMSERVICE)
	systemctl enable batmon.service
	systemctl start batmon.service

uninstall:
	@echo "==> Removing the service..."
	# Remove the service file
	systemctl stop batmon.service
	systemctl disable batmon.service
	rm -f $(BMSERVICE)

	@echo "==> Removing the files..."
	# Remove the binaries
	rm -f $(BIN)
	# Delete the configuration files
	rm -rf $(CONF)

	@echo "==> Deleting the batmon user, group, and home directory..."
	# Delete the user, home directory, and group
	deluser --remove-home $(USER)

