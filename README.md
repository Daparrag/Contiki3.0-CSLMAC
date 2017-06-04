The Contiki Operating System
============================

[![Build Status](https://travis-ci.org/contiki-os/contiki.svg?branch=master)](https://travis-ci.org/contiki-os/contiki/branches)

Contiki is an open source operating system that runs on tiny low-power
microcontrollers and makes it possible to develop applications that
make efficient use of the hardware while providing standardized
low-power wireless communication for a range of hardware platforms.

Contiki is used in numerous commercial and non-commercial systems,
such as city sound monitoring, street lights, networked electrical
power meters, industrial monitoring, radiation monitoring,
construction site monitoring, alarm systems, remote house monitoring,
and so on.

For more information, see the Contiki website:

[http://contiki-os.org](http://contiki-os.org)


--------------
## CORDINATE SAMPLED LISTENING PROTOCOL(CSL)
[Coordinate Sampled Listening protocol (CSL)](https://github.com/Daparrag/Contiki3.0-CSLMAC/tree/master/core/net/mac/cslmac)

This implementation include the **standard 802.15.4e CSL radio duty cycle protocol** for wireless sensor networks over **_contiki-OS_** the follow figure show the asynchronous and asynchronous states of the protocol. 

<img src="https://github.com/Daparrag/Contiki3.0-CSLMAC/blob/master/info/CSL1.png" alt="Coordinate_sample_listen protocol" width="700px" />

This implementation has been compared with the current  Contiki **RDC protocols** _ContikiMAC_, and _XMAC_  obtined the follow results using a star topology

### Average radio duty cycle for diferent sample rates in function of the number of nodes.
<img src="https://github.com/Daparrag/Contiki3.0-CSLMAC/blob/master/info/CSL2.png" alt="average radio duty cycle sampling" width="700px" />

### Average successful transmition time in function of the number of nodes

<img src="https://github.com/Daparrag/Contiki3.0-CSLMAC/blob/master/info/CSLTX.png" alt="average transmission time" width="700px" />

### Packet reception probability measure at the receptor

<img src="https://github.com/Daparrag/Contiki3.0-CSLMAC/blob/master/info/Probability.png" alt="packet reception probability" width="700px" />









