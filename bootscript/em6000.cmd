setenv kernel_loadaddr 0x48000000
setenv script_loadaddr 0x43000000
setenv console 'ttyS0,115200n8'
setenv nandargs 'setenv bootargs console=${console} init=/linuxrc mtdparts=mtd-nand-sunxi.0:1M,4M,3M,3M,3M,3M,8M,128M,- ubi.mtd=7 root=ubi0:rootfs rootwait rootfstype=ubifs quiet'
setenv nandboot 'run nandargs; nand read ${script_loadaddr} 0xe00000 0x10000; nand read ${kernel_loadaddr} 0x1100000 0x400000; bootm ${kernel_loadaddr}'
setenv bootcmd 'run nandboot'
setenv bootdelay 5

