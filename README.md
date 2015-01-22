mmdio_test a simple NIC latency tool
====================================

Abstract
--------
This is code to measure latency from various NIC's. Of course buses and
other things are included.

History
-------
This code was used as "tool" for driver and performance understanding.
Harald Welte, Jamal Hadi Salim, Grant Grundler, Lennert Bytenheck and
probably som Intel folks. Dave M? Jessie?. Feel free to remind me.

Introduction
-----------
From code comment from 2005:
Part of it is that uncached accesses are plain slow.  An L2 miss is ~370
cycles on my hardware (155ns*2.4 confirms that), but an uncached access
to the same memory location is consistently ~490 cycles.

And part of it seems to be the e1000.  Reading the device control register
(E1000_CTRL, offset 0x00) is ~1700 cycles each, but reading the interrupt
cause register (E1000_ICR, 0xc0) is ~2100 cycles each.  The interrupt mask
register (E1000_IMS, 0x100) is also ~2100 cycles each.

Status
------
Worked 2005. :) But should be updated for recent adapters. Legacy support 
for e1000 and tg3. 

Copyright
---------
Open-Source via GPL. See code.

Authors
-------					
Robert Olsson <robert@Radio-Sensors.COM>
Harald Welte

