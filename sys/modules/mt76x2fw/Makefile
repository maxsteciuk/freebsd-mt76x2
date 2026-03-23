KMOD=	mt76x2fw
FIRMWS=	mt7662u.bin:mt76x2_fw:1 mt7662u_rom_patch.bin:mt76x2_rom_patch:1

CLEANFILES=	mt7662u.bin mt7662u_rom_patch.bin

mt7662u.bin: ${SRCTOP}/sys/contrib/dev/mt76x2/mt7662u.bin.uu
	uudecode -p ${.ALLSRC} > ${.TARGET}

mt7662u_rom_patch.bin: ${SRCTOP}/sys/contrib/dev/mt76x2/mt7662u_rom_patch.bin.uu
	uudecode -p ${.ALLSRC} > ${.TARGET}

.include <bsd.kmod.mk>
