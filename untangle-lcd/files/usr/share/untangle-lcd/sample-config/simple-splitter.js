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
        "splitter_name" : "cacher",
        "params" : {
            "cache_creation_timeout" : 2592000, /* 60*60*24*30 */ 
            "cache_access_timeout" : 7200,  /* 60*60*2 */
            "cache_max_size" : 2000,
            "cache_hard_max_size" : 50000, 
            "cache_clean_interval" : 1200
        }
    },{
        /* Evenly distribute 50 points to each connection */
        "splitter_name" : "basic",
        "params" : {
            "scores" : [ 50, 0, 50 ]
        }
    }]
}
