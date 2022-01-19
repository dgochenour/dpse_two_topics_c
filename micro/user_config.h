/*
* (c) Copyright, Real-Time Innovations, 2021.  All rights reserved.
* RTI grants Licensee a license to use, modify, compile, and create derivative
* works of the software solely for use with RTI Connext DDS. Licensee may
* redistribute copies of the software provided that all such copies are subject
* to this license. The software is provided "as is", with no warranty of any
* type, including any warranty for fitness for any purpose. RTI is under no
* obligation to maintain or support the software. RTI shall not be liable for
* any incidental or consequential damages arising out of the use or inability
* to use the software.
*/

#ifndef USER_CONFIG_H
#define USER_CONFIG_H

// user-configurable values
char *peer = "127.0.0.1";
int domain_id = 100;

// Typical values on Win10 host
// char *loopback_name = "Loopback Pseudo-Interface 1";
// char *eth_nic_name = "Wireless LAN adapter Wi-Fi";

// Typical values on Ubuntu 20.04 host
char *loopback_name = "lo";
char *eth_nic_name = "wlp0s20f3";

#endif