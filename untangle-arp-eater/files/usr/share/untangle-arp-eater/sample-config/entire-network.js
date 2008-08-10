/**
 * This is a sample designed to take over the local subnets 
 */
{
  "gateway" : "0.0.0.0",

  /* By default, spoof a host for 5 seconds after not seeing any traffic */
  "timeout" : 5.0,

  /* By default spoof a host every 2 seconds */
  "rate" : 2.0,

  /* Send broadcast spoof */
  "broadcast" : true,

  "networks" : [{
      "enabled" : true,
      "ip" : "0.0.0.0",
      "netmask" : "0.0.0.0",
      "spoof" : true,
      "passive" : true,
      "target" : "0.0.0.0"
  }]
}
