{ 
    "enabled" : true,

    "interface_min" : 10000,
    
    "interfaces" : [{
        "os_name" : "eth0",
        "alpaca_interface_id" : 1        
    },{
        "os_name" : "eth2",
        "alpaca_interface_id" : 3
    }],

    "tests" : [{
        "alpaca_interface_id" : 1,
        "test_class_name" : "dns",
        "timeout" : 2000,
        "delay" : 4000,
        "bucket_size" : 10,
        /* So, on complete failure, it would take 28 seconds to
        recover.  * It would take 14 seconds (12 + 2) before a failure
        is detected. */
        "threshold" : 7,
        "params" : {
            /* DNS Doesn't require any params */
        }
    },{
        "alpaca_interface_id" : 2,
        "test_class_name" : "dns",
        "timeout" : 2000,
        "delay" : 4000,
        "bucket_size" : 10,
        /* So, on complete failure, it would take 28 seconds to
        recover.  * It would take 14 seconds (12 + 2) before a failure
        is detected. */
        "threshold" : 7,
        "params" : {
            /* DNS Doesn't require any params */
        }
    },{
        "alpaca_interface_id" : 1,
        "test_class_name" : "arp",
        "timeout" : 2000,
        "delay" : 4000,
        "bucket_size" : 10,
        /* So, on complete failure, it would take 28 seconds to
        recover.  * It would take 14 seconds (12 + 2) before a failure
        is detected. */
        "threshold" : 7,
        "params" : {
            /* ARP doesn't require any params */
        }
    }]
}
