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