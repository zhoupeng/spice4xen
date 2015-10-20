# About Spice #

http://spice-space.org/

# About Porting Spice to Xen #


It is part of our skylark project.

# Quick-Start Guide #

> You can get the source from the [svn](http://code.google.com/p/spice4xen/source/checkout) or [tarball](http://code.google.com/p/spice4xen/downloads/list)

1. Build and install
```
   set CONFIG_SPICE  ?= y
   $cd the root of spice4xen
   $make install-xen
   $make install-kernels
   $make install-tools
```
> Please refer to the documents of Xen for details.

2. Create a configure file for xm

An example:
```
kernel = '/usr/lib/xen/boot/hvmloader'
builder = 'hvm'
memory = '384'
device_model = '/usr/lib/xen/bin/qemu-dm'
# Disks
disk = [ 'file:path-to-diskimage/imagefile.img,ioemu:hda,w', 'file:path-to-cd/cd_imagefile.iso,ioemu:hdc:cdrom,r' ]
# Networking
vif = ['ip=192.168.1.112, type=ioemu, bridge=xenbr0, mac=00:21:97:CB:0E:7D']
#Behaviour
boot='cd'
vnc=1
serial = 'pty'
vcpus=1
# Hostname
name = 'UserVM'
usbdevice='tablet'
# Spice
spice=1
spicehost='192.168.1.187'
spiceport=6000
spicepasswd = 'password'
spice_disable_ticketing = 0 # 0|1
spiceic = 'auto_glz' # auto_glz|auto_lz|quic|glz|lz|off(default=on=auto_glz) # image compression
spicesv = 'on' # on|off|all|filter# streaming video detection and (lossy) compression, on is added as tag which is default.
spicejpeg_wan_compression = 'auto' # auto|never|always
spicezlib_glz_wan_compression = 'auto' # auto|never|always
#spiceplayback = 1 # 1-on|0-off # playback compression, should using CELT algorithm?
spiceagent_mouse = 1 # 1-on (default)| 0-off, # agent-mouse mode
#qxl=0
#qxlnum=1
#qxlram=64 # M
```
3. Create a HVM
```
$xm create path-to-configure-file/xm.cfg
```
4. Launch the spice client

According to the example configuration above:
```
$/usr/local/spice-xen/bin/spicec -h 192.168.1.187 -p 6000 -w password
```