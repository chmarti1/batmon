# Installation target locations
# Edit these options prior to installation to change options
# Do not leave trailing / separators
BIN=/usr/bin
SYSTEMD=/etc/systemd/system
HOME=/var/batmon
CONF=/etc

# Build dependent parameters
BMCMD=$(BIN)/bmcmd
BATMON=$(BIN)/batmon
BMCONF=$(CONF)/batmon.conf
BMSERVICE=$(SYSTEMD)/batmon.service
USER=batmon

# Build some dependency lists
LINK=-lm -llabjackusb
BINARIES=bmcmd.bin batmon.bin


bmsvc.bin: bmsvc.c chargemodel.o autocom.o
	@echo "==> Compiling the batery monitor service binary"
	gcc -Wall -o bmsvc.bin autocom.o chargemodel.o bmsvc.c -lm -llabjackusb

chargemodel.o: chargemodel.c chargemodel.h
	@echo "==> Compiling the chargemodel object"
	gcc -Wall -c chargemodel.c -o chargemodel.o

autocom.o: autocom.c autocom.h
	@echo "==> Compiling the autocom binary"
	gcc -Wall -c autocom.c -o autocom.o

clean:
	rm -f *.bin
	rm -f *.o

install: $(BINARIES)
	@echo "==> Installing in the system..."
	@echo "==> Creating the batmon user and home dir"
	# Create the bmcmd user and home directory
	adduser --system --group --home $(HOME) $(USER)
	chmod 775 $(HOME)

	@echo "==> Placing the files..."
	# Install the bmcmd binary
	cp -f bmcmd.bin $(BMCMD)
	chown root:root $(BMCMD)
	chmod 775 $(BMCMD)

	# Install the batmon binary
	cp -f batmon.bin $(BATMON)
	chown root:root $(BATMON)
	chmod 770 $(BATMON)

	# Install the configuration file
	cp -f batmon.conf $(BMCONF)
	chown root:batmon $(BMCONF)

	# Place the service unit file and start the service
	cp -f batmon.service $(BMSERVICE)
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
	rm -f $(BMCMD)
	rm -f $(BATMON)
	# Delete the configuration file
	rm -f $(BMCONF)

	@echo "==> Deleting the batmon user, group, and home directory..."
	# Delete the user, home directory, and group
	deluser --remove-home $(USER)
	delgroup $(USER)

