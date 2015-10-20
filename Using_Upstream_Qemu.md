  * Use [this patch](http://spice4xen.googlecode.com/files/spice-upstreamXen-upstreamQemu.diff) for unstable-xen or xen-4.1.0

  * Get and install [libspiceserver and spice-protocol](http://spice-space.org/)

  * Get Anthony's tree for xen-upstream-qemu from git://xenbits.xen.org/people/aperard/qemu-dm.git

  * Build and install the qemu with the following config options
```
 ./configure --target-list=i386-softmmu --enable-spice
   --enable-xen --extra-cflags=-I${path-to-xen}/dist/install/usr/include
   --extra-ldflags=-L${path-to-xen}/dist/install/usr/lib
```

  * Set the VM configuration file.

> Add spice fields in VM configuration file.
```
    #spice
    spice=1
    spiceport=6000
    spicehost='192.168.1.187'
    spicedisable_ticketing = 0 # default is 0
    spicepasswd = 'password'

    apic=0 # disable acpi, if you used the additional patch referred below, use acpi=0 instead
```

> You may need to disable acpi(I'm not sure),

> but if you want to try to disable acpi, you may need to set

> apic = 0, (Yes, it is apic not acpi, pls don't ask me why, because I am also confused with it).

> If you feel uncomfortable by set apic = 0, you can try [the additional patch](http://spice4xen.googlecode.com/files/apic-acpi.diff),

> then you can use acpi=0 in vm cfg file instead of apic to give "no-acpi"   argument to qemu.

  * Use xl to create an hvm

  * Because xen-upstream-qemu don't support graphic, pls try this in linux-hvm disabling graphic boot.