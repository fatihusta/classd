{ 
    "enabled" : true,

    /* Log the splits once a minute */
    "log_interval" : 60,
    
    "uplinks" : [{
        "os_name" : "eth0",
        "alpaca_interface_id" : 1
    },{
        "os_name" : "eth2",
        "alpaca_interface_id" : 3
    }],
    
    "splitters" : [{
        /* Disable all interfaces that are not online */
        "splitter_name" : "online",
        "params" : {}
    },{
        "splitter_name" : "basic",
        "params" : {
            "scores" : [ 500, 50, 120 ]
        }
    }]
}
