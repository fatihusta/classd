/**
 * This is a sample where 192.168.1.17 should not be spoofed, and
 * anything outside of the range of 192.168.1.0/24 should also not be
 * spoofed.  Only addresses that are seen on the network are ARP
 * spoofed, so if a machine never sends any broadcast traffic
 * (unlikely) it will not be spoofed.
 */
{
  gateway : "0.0.0.0",
  inteface : "eth0",
      
  hosts : [{
      /* Lie that you are 192.168.1.17 always, regardless of whether
       * or not you see traffic about it. */
      enabled : true,
      ip : "192.168.1.17",
      netmask : "255.255.255.0",
      spoof : false,
      opportunistic : false,
      target : "0.0.0.0"
  },{
      enabled : true,
      ip : "192.168.1.0",
      netmask : "255.255.255.0",
      spoof : true,
      opportunistic : true,
      target : "0.0.0.0"
  },{
      enabled : true,
      network : "0.0.0.0 / 0",
      spoof : false,
      opportunistic : true,
      target : "0.0.0.0"
  }]
}
