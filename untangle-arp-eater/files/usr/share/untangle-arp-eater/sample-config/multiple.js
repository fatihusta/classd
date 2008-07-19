/**
 * This is a sample designed to take over two specific subnets (The default is to NOT Spoof),
 */
{
  gateway : "automatic",

  /* By default, spoof a host for 5 seconds after not seeing any traffic */
  timeout : 5.0,

  /* By default spoof a host every 2 seconds */
  rate : 2.0,

  hosts : [{
      /* This should tell all hosts on 192.168.1.0 that we are 192.168.1.1, and tell
       * 192.168.1.1 that we are the hosts in our list. */
      enabled : true,
      network : "192.168.1.0 / 24",
      spoof : true,
      opportunistic : true,
      target : "192.168.1.1"
  },{
      /* This should tell all hosts on 192.168.2.0 that we are 192.168.2.1, and tell
       * 192.168.2.1 that we are the hosts in our list. */
      enabled : true,
      network : "192.168.2.0 / 24",
      spoof : true,
      opportunistic : true,
      target : "192.168.2.1",
      timeout : 1.5,
      rate : 1.0
  }]
}
