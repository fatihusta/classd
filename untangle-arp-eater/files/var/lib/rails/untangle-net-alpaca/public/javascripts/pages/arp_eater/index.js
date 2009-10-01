Ext.ns('Ung');
Ext.ns('Ung.Alpaca.Pages');
Ext.ns('Ung.Alpaca.Pages.ArpEater');

if ( Ung.Alpaca.Glue.hasPageRenderer( "arp_eater", "index" )) {
    Ung.Alpaca.Util.stopLoading();
}

Ung.Alpaca.Pages.ArpEater.Index = Ext.extend( Ung.Alpaca.PagePanel, {
    initComponent : function()
    {
        var networks = this.settings["networks"];
        for ( var c = 0 ; c < networks.length ; c++ ) {
            var network = networks[c];
            network.network = network.ip + "/" + network.netmask;
        }

        var enabledColumn = new Ung.Alpaca.grid.CheckColumn({
            header : this._( "On" ),
            dataIndex : "enabled",
            sortable : false,
            fixed : true
        });
        
        var spoofColumn = new Ung.Alpaca.grid.CheckColumn({
            header : this._( "Re-Route" ),
            dataIndex : "spoof",
            sortable : false,
            width : 70,
            fixed : true
        });
        
        var passiveColumn = new Ung.Alpaca.grid.CheckColumn({
            header : this._( "Passive" ),
            dataIndex : "passive",
            sortable : false,
            fixed : true
        });

        this.networksGrid = new Ung.Alpaca.EditorGridPanel({
            settings : this.settings,
            recordFields : [ "enabled", "spoof", "passive", "network", "gateway", "description", "is_spoof_host_enabled" ],
            selectable : true,
            sortable : false,
            hasReorder : true,

            /* Name must set in order to get and set the settings */
            name : "networks",

            recordDefaults : {
                enabled : true,
                spoof : false,
                passive : true,
                network : "1.2.3.4/32",
                gateway : "auto",
                description : this._( "[no description]" )
            },

            plugins : [enabledColumn, spoofColumn, passiveColumn ],
            columns : [enabledColumn, spoofColumn, passiveColumn, {
                header : this._( "Network" ),
                width : 200,
                sortable :  false,
                dataIndex : "network",
                editor  : new Ext.form.TextField({
                    allowBlank : false
                })
            },{
                header : this._( "Gateway" ),
                width : 200,
                sortable :  false,
                dataIndex : "gateway",
                editor  : new Ext.form.TextField({
                    allowBlank : false
                })
            },{
                header : this._( "Description" ),
                width : 200,
                sortable :  false,
                dataIndex : "description",
                editor  : new Ext.form.TextField({
                    allowBlank : false
                })
            }]
        });

        this.networksGrid.store.load();

        enabledColumn = new Ung.Alpaca.grid.CheckColumn({
            header : this._( "On" ),
            dataIndex : "enabled",
            sortable : false,
            fixed : true
        });
                
        passiveColumn = new Ung.Alpaca.grid.CheckColumn({
            header : this._( "Passive" ),
            dataIndex : "passive",
            sortable : false,
            fixed : true
        });
        
        /* Make these non-editable */
        enabledColumn.onMouseDown = null;
        passiveColumn.onMouseDown = null;

        this.activeGrid = new Ung.Alpaca.EditorGridPanel({
            settings : this.settings,

            recordFields : [ "enabled", "passive", "address", "gateway" ],
            
            tbar : [{
                text : this._( "Refresh" ),
                handler : this.refreshActiveEntries,
                scope : this
            }],
            
            name : "active_hosts",
            saveData : false,
            
            plugins : [ enabledColumn, passiveColumn ],
            columns : [ enabledColumn, passiveColumn, {
                header : this._( "Address" ),
                width: 200,
                sortable: true,
                dataIndex : "address"
            },{
                header : this._( "Gateway" ),
                width: 200,
                sortable: true,
                dataIndex : "gateway"
            }]
        });

        this.activeGrid.store.load();
        
        Ext.apply( this, {
            items : [{
                xtype : "label",
                html : this._( "Re-Router" ),
                cls: 'page-header-text'     
            },{
                xtype : "fieldset",
                autoHeight : true,
                items : [{
                    xtype : "checkbox",
                    fieldLabel :  this._( "Enabled" ),
                    name : "arp_eater_settings.enabled"
                },{
                    xtype : "checkbox",
                    fieldLabel :  this._( "Broadcast" ),
                    name : "arp_eater_settings.broadcast"
                },{
                    xtype : "textfield",
                    fieldLabel :  this._( "Gateway" ),
                    boxLabel : this._( "(blank or 'auto' for automatic)" ),
                    name : "arp_eater_settings.gateway"
                }]
            },{
                xtype : "label",
                html : this._( "Networks" ),
                cls : 'label-section-heading-2'
            }, this.networksGrid, {
                xtype : "label",
                html : this._( "Active Hosts" ),
                cls: 'label-section-heading-2'                                
            }, this.activeGrid ]
        });
        
        Ung.Alpaca.Pages.ArpEater.Index.superclass.initComponent.apply( this, arguments );
    },

    saveMethod : "/arp_eater/set_settings",

    updateSettings : function( settings )
    {
        Ung.Alpaca.Pages.ArpEater.Index.superclass.updateSettings.apply( this, arguments );

        var networks = settings["networks"];
        for ( var c = 0 ; c < networks.length ; c++ ) {
            var network = networks[c];
            var networkString = network["network"].split( "/" );
            network.ip = networkString[0];
            network.netmask = ( networkString.length > 1 ) ? networkString[1] : "";

            delete( network["network"] );
        }
    },

    refreshActiveEntries : function()
    {
        var handler = this.completeRefreshActiveEntries.createDelegate( this );
        Ung.Alpaca.Util.executeRemoteFunction( "/arp_eater/get_active_hosts", handler );
    },

    completeRefreshActiveEntries : function( activeHosts, response, options )
    {
        if ( !activeHosts ) return;

        this.activeGrid.store.loadData( activeHosts );
    }
});

Ung.Alpaca.Pages.ArpEater.Index.settingsMethod = "/arp_eater/get_settings";
Ung.Alpaca.Glue.registerPageRenderer( "arp_eater", "index", Ung.Alpaca.Pages.ArpEater.Index );
