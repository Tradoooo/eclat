# Éclat — image QNX 8.0 x86_64 pour la preuve ECL-14 (politique de sécurité en vigueur).
#
# Le noyau, un shell, les outils de sécurité, les binaires Éclat, et la politique compilée
# embarquée à /proc/boot/secpol.bin — poussée dans procnto au démarrage par secpolpush.
# La console série de la VM est la preuve : aucune sortie n'est retouchée.
#
# GABARIT : la ligne __QNX_TARGET__ est remplacée par tools/demo-secpol-on-qnx.sh avec le
# QNX_TARGET réellement sourcé. mkifs ne résout pas cette variable dans le champ [search].
[search=.:__QNX_TARGET__/x86_64:__QNX_TARGET__/x86_64/boot/sys:]
[+dupignore]
[+optional]
[uid=0 gid=0 dperms=0555 perms=0555]
[image=24m]
[virtual=x86_64,multiboot ] boot = {
    startup-x86 -D8250..115200 -zz -f 0,0,1000M
    PATH=/proc/boot:/system/bin
    LD_LIBRARY_PATH=/proc/boot:/system/lib:/system/lib/dll
    [+keeplinked uid=0 gid=0 perms=0700 ] CONFIG_PATH=/proc/boot:/system/etc procnto-smp-instr -mr -d 022 -u 066 -F 2000
}

[+script] startup-script = {
    procmgr_symlink /proc/boot/ksh /bin/sh
    procmgr_symlink /proc/boot/ksh /bin/ksh
    procmgr_symlink /var/tmp /tmp
    procmgr_symlink /proc/boot/ldqnx-64.so.2 /usr/lib/ldqnx-64.so.2
    procmgr_symlink /proc/boot/pidin /bin/pidin
    PATH=/proc/boot:/system/bin
    LD_LIBRARY_PATH=/proc/boot:/system/lib:/system/lib/dll
    HOME=/proc/boot
    TERM=qansi
    devc-con
    devc-ser8250 -e
    waitfor /dev/ser1
    display_msg "ECLAT ECL-14 - politique de securite QNX 8.0"
    on -d -t /dev/ser1 -e HOME=/proc/boot ksh -c /proc/boot/demo-secpol.sh
}

multiboot.boot = boot/sys/multiboot.boot
procnto-smp-instr = boot/sys/procnto-smp-instr
startup-x86 = boot/sys/startup-x86

eclat-broker = ./eclat-broker
eclat-client = ./eclat-client
eclat-sonde = ./eclat-sonde
secpol.bin = ./secpol.bin
demo-secpol.sh = ./demo-secpol.sh
politique.secpol = ./politique.secpol
pol-broker.toml = ./pol-broker.toml

devc-con = sbin/devc-con
devc-ser8250 = sbin/devc-ser8250
on = bin/on
sh = bin/sh
ksh = bin/ksh
pidin = bin/pidin
secpolpush = bin/secpolpush
secpolmonitor = bin/secpolmonitor
slay = bin/slay
toybox = usr/bin/toybox
[type=link] /proc/boot/cat=/proc/boot/toybox
[type=link] /proc/boot/sleep=/proc/boot/toybox
[type=link] /proc/boot/ls=/proc/boot/toybox
[type=link] /proc/boot/echo=/proc/boot/toybox
[type=link] /proc/boot/head=/proc/boot/toybox

ldqnx-64.so.2 = usr/lib/ldqnx-64.so.2
libc.so.6 = lib/libc.so.6
libgcc_s.so.1 = lib/libgcc_s.so.1
libm.so.3 = lib/libm.so.3
libsecpol.so.1 = lib/libsecpol.so.1
libsocket.so.4 = lib/libsocket.so.4
libfsnotify.so.1 = lib/libfsnotify.so.1
libz.so.2 = usr/lib/libz.so.2