# $FreeBSD$

S=${SRCTOP}/sys

.PATH: ${.CURDIR}/../../dev/usb/wlan

KMOD=if_mt76x2
SRCS=opt_bus.h opt_usb.h opt_wlan.h device_if.h bus_if.h usb_if.h \
usbdevs.h if_mt76x2.c

.include <bsd.kmod.mk>
