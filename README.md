# PVmapper resolves PV names to IOC servers

PVmapper project started after arising challenges to control the broadcast
floods in a saturated EPICS environment. With emerging number of busy EPICS
operator screens, misbehaving Channel Access clients, and the tendency to
develop run-once tools for reporting or AI needs, the EPICS broadcast traffic
seems to be very intense and on occasions detremental to the health of the
controls network infrastructure.

Many facilities utilizing EPICS have come to similar conclusions in the past.
Various ideas have emerged how to address the issue, including the original
[CA-nameserver](https://epics.anl.gov/extensions/nameserver/index.php).
Lately a more modern Java implementation has been
[cfNameserver](https://github.com/ChannelFinder/cfNameserver) implemented
and is part of the ChannelFinder and recsync feature rich framework.

The goal of this project is to be a simple yet reliable EPICS name server
that works seamlessly in any existing EPICS environment. It aims to build
on original EPICS ideology of self discovering environment, but consolidating
broadcast traffic for reduced resources footprint. Aside from simplicity, the
motivation for this project were:
* reduce EPICS broadcast footprint
* support any IOC (including LabView, pcaspy etc.) without modifying it
* recognize new IOCs dynamically
