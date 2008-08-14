
module ArpEaterHelper
  class NetworkTableModel < Alpaca::Table::TableModel
    include Singleton

    def initialize
      columns = []
      columns << Alpaca::Table::DragColumn.new
      columns << Alpaca::Table::Column.new( "enabled", "On".t ) do |entry,options|
        row_id = options[:row_id]
        view = options[:view]

<<EOF
        #{view.hidden_field_tag( "entries[]", row_id )}
        #{view.table_checkbox( row_id, "enabled", entry.enabled )}
EOF
      end

      columns << Alpaca::Table::Column.new( "spoof", "Spoof".t ) do |entry,options|
        row_id = options[:row_id]
        view = options[:view]

<<EOF
        #{view.table_checkbox( row_id, "spoof", entry.spoof )}
EOF
      end

      columns << Alpaca::Table::Column.new( "passive", "Passive".t ) do |entry,options|
        row_id = options[:row_id]
        view = options[:view]

<<EOF
        #{view.table_checkbox( row_id, "passive", entry.passive )}
EOF
      end

      columns << Alpaca::Table::Column.new( "network", "Network" ) do |entry,options|
        row_id = options[:row_id]
        view = options[:view]
<<EOF
        #{view.text_field( "network", row_id, { :value => "#{entry.ip} / #{entry.netmask}" } )}
EOF
      end

      columns << Alpaca::Table::Column.new( "gateway", "Gateway" ) do |entry,options|
        row_id = options[:row_id]
        view = options[:view]
<<EOF
        #{view.text_field( "gateway", row_id, { :value => entry.gateway } )}
EOF
      end

      columns << Alpaca::Table::Column.new( "description fill", "Description" ) do |static_entry,options| 
        "" +
        options[:view].text_field( "description", options[:row_id], { :value => static_entry.description } )
      end
      
      columns << Alpaca::Table::DeleteColumn.new
      
      super(  "ArpEater", "arp-eater-networks", "", "arp-eater-network", columns )
    end

    def row_id( row )
      "row-#{rand( 0x100000000 )}"
    end

    def action( table_data, view )
      <<EOF
<div onclick="#{view.remote_function( :url => { :action => :create_network } )}" class="add-button">
  #{"Add".t}
</div>
EOF
    end
  end

  def network_table_model
    NetworkTableModel.instance
  end

  class ActiveHostTableModel < Alpaca::Table::TableModel
    include Singleton

    def initialize
      columns = []
      columns << Alpaca::Table::Column.new( "enabled", "On".t ) do |entry,options|
        row_id = options[:row_id]
        view = options[:view]

<<EOF
        #{view.table_checkbox( row_id, "enabled", entry.enabled, true )}
EOF
      end

      columns << Alpaca::Table::Column.new( "passive", "Passive".t ) do |entry,options|
        row_id = options[:row_id]
        view = options[:view]

<<EOF
        #{view.table_checkbox( row_id, "passive",  entry.passive, true )}
EOF
      end

      columns << Alpaca::Table::Column.new( "address", "Address" ) do |entry,options|
        "<span>#{entry.address}</span>"
      end

      columns << Alpaca::Table::Column.new( "gateway", "Gateway" ) do |entry,options|
        "<span>#{entry.gateway}</span>"
      end
      
      super(  "ArpEater", "arp-eater-active-hosts", "", "arp-eater-active-host read-only", columns )
    end

    def row_id( row )
      "row-#{rand( 0x100000000 )}"
    end

    def action( table_data, view )
      <<EOF
<div onclick="#{view.remote_function( :url => { :action => :refresh_active_hosts } )}" class="add-button">
  #{"Refresh".t}
</div>
EOF
    end
  end

  def active_host_table_model
    ActiveHostTableModel.instance
  end
end
