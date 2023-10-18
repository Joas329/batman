CC = gcc
CFLAGS = -DWITH_UPOWER -DWITH_WLRDISPLAY -DWITH_GETINFO `pkg-config --cflags upower-glib gtk4 libadwaita-1`
LDFLAGS = -lwayland-client `pkg-config --libs upower-glib gtk4 libadwaita-1`
CFLAGS_NFCD = -fPIC -DNFC_PLUGIN_EXTERNAL `pkg-config --cflags nfcd-plugin libglibutil gobject-2.0 glib-2.0`
LDFLAGS_NFCD = -fPIC -shared `pkg-config --libs libglibutil gobject-2.0 glib-2.0` -lwayland-client

TARGET = batman
TARGET_HELPER = batman-helper
TARGET_GUI = batman-gui
TARGET_GOVERNOR = governor
TARGET_LIB = libbatman-wrappers.so
TARGET_LIBPOWER = batman-libpower
TARGET_NFCD = batman.so

SRC_HELPER = src/batman-helper.c src/wlrdisplay.c src/batman-wrappers.c src/getinfo.c
SRC_GUI = src/batman-gui.c src/configcontrol.c src/getinfo.c
SRC_GOVERNOR = src/governor.c src/wlrdisplay.c
SRC_LIB = src/batman-wrappers.c src/wlrdisplay.c src/getinfo.c
SRC_LIBPOWER = src/batman-libpower.c
SRC_NFCD = src/nfcd-batman-plugin.c src/wlrdisplay.c
HEADERS = src/batman-wrappers.h src/getinfo.h src/governor.h

BINDIR = /usr/bin
CONFIGDIR = /var/lib/batman
SYSTEMD_DIR = /usr/lib/systemd/system
OPENRC_DIR = /etc/init.d
DESKTOP_DIR = /usr/share/applications
ICON_DIR = /usr/share/icons
INCLUDE_DIR = /usr/include/batman

.PHONY: all
all: $(TARGET) $(TARGET_LIBPOWER) $(TARGET_NFCD)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC_HELPER) $(LDFLAGS) -o $(TARGET_HELPER)
	$(CC) $(CFLAGS) $(SRC_GUI) $(LDFLAGS) -o $(TARGET_GUI)
	$(CC) $(CFLAGS) $(SRC_GOVERNOR) $(LDFLAGS) -o $(TARGET_GOVERNOR)
	$(CC) -fPIC -shared $(CFLAGS) $(SRC_LIB) $(LDFLAGS) -o $(TARGET_LIB)

$(TARGET_LIBPOWER):
	gcc $(SRC_LIBPOWER) -o $(TARGET_LIBPOWER) `pkg-config --libs --cflags libgbinder`

$(TARGET_NFCD): nfcd-batman-plugin.o wlrdisplay.o
	$(CC) $^ $(LDFLAGS_NFCD) -o $@

nfcd-batman-plugin.o: src/nfcd-batman-plugin.c
	$(CC) -c $< $(CFLAGS_NFCD) -O2 -o $@

wlrdisplay.o: src/wlrdisplay.c
	$(CC) -c $< $(CFLAGS_NFCD) -O2 -o $@

.PHONY: install
install: $(TARGET)
	cp src/$(TARGET) $(BINDIR)
	cp $(TARGET_HELPER) $(BINDIR)
	cp $(TARGET_GUI) $(BINDIR)
	cp $(TARGET_GOVERNOR) $(BINDIR)
	cp $(TARGET_LIB) /usr/lib
	cp $(TARGET_LIBPOWER) $(BINDIR)

	cp data/batman-gui.desktop $(DESKTOP_DIR)
	cp data/batman.png $(ICON_DIR)

	mkdir -p $(CONFIGDIR)
	cp data/config $(CONFIGDIR)
	chmod +x $(BINDIR)/$(TARGET) $(BINDIR)/$(TARGET_HELPER) $(BINDIR)/$(TARGET_GUI) $(BINDIR)/$(TARGET_GOVERNOR)

	mkdir -p $(INCLUDE_DIR)
	cp $(HEADERS) $(INCLUDE_DIR)

ifeq ($(shell test -d $(SYSTEMD_DIR) && echo 1),1)
	cp data/batman.service $(SYSTEMD_DIR)
else ifeq ($(shell test -e /sbin/openrc && echo 1),1)
	cp data/batman.rc $(OPENRC_DIR)/batman
else
	cp data/batman-init $(OPENRC_DIR)/batman
endif

.PHONY: clean
clean:
	rm -f $(TARGET_HELPER)
	rm -f $(TARGET_GUI)
	rm -f $(TARGET_GOVERNOR)
	rm -f $(TARGET_LIB)
	rm -f $(TARGET_LIBPOWER)
	rm -f $(TARGET_NFCD)
	rm -f nfcd-batman-plugin.o wlrdisplay.o
