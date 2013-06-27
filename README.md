FreeRadius module to support authentication using a Time-base One Time Password (TOTP) as the password in a CHAP communication.  Includes proof of concept for baking this into a chillispot on OpenWRT and chef recipes for creating the OpenWRT build environment in a Vagrant virtual machine.

Configure command for OpenWRT build:

./configure --target=mips-openwrt-linux --host=mips-openwrt-linux
http://lgallardo.com/en/2011/05/19/compilacion-cruzada-cross-compiling-de-mips-para-openwrt/
