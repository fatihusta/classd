{ 
    "enabled" : true,
    
    /* Log the splits once a minute */
    "log_interval" : 60,
    
    "uplinks" : [{
        "os_name" : "eth0",
        "alpaca_interface_id" : 1
    },{
        "os_name" : "eth2",
        "alpaca_interface_id" : 2
    }],
    
    /* Evenly distribute 50 points to each connection */
    "splitters" : [{
        "splitter_name" : "basic",
        "params" : {
            "distribution" : [ 50, 50 ]
        }
    }]
}
