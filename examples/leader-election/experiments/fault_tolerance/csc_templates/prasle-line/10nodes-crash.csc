<?xml version="1.0" encoding="UTF-8"?>
<simconf version="2024090601">
  <simulation>
    <title>PRASLE Leader Election - 10 Nodes (Line Topology) - Leader Crash Scenario</title>
    <randomseed>123456</randomseed>
    <motedelay_us>1000000</motedelay_us>
    <radiomedium>
      org.contikios.cooja.radiomediums.UDGM
      <transmitting_range>52.0</transmitting_range>
      <interference_range>52.0</interference_range>
      <success_ratio_tx>1.0</success_ratio_tx>
      <success_ratio_rx>1.0</success_ratio_rx>
    </radiomedium>
    <events>
      <logoutput>40000</logoutput>
    </events>
    <motetype>
      org.contikios.cooja.contikimote.ContikiMoteType
      <identifier>prasle1</identifier>
      <description>PRASLE Node Type</description>
      <source>[CONTIKI_DIR]/examples/leader-election/prasle-node.c</source>
      <commands>$(MAKE) -j$(CPUS) prasle-node.cooja TARGET=cooja ALGORITHM=prasle TOPOLOGY=line NETWORK_SIZE=10</commands>
      <firmware>[CONTIKI_DIR]/examples/leader-election/build/cooja/prasle-node.cooja</firmware>
      <moteinterface>org.contikios.cooja.interfaces.Position</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.Battery</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiVib</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiMoteID</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiRS232</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiBeeper</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.IPAddress</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiRadio</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiButton</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiPIR</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiClock</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiLED</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiCFS</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiEEPROM</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.Mote2MoteRelations</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.MoteAttributes</moteinterface>
    </motetype>
    <mote>
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>20.0</x>
        <y>100.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiMoteID
        <id>1</id>
      </interface_config>
      <motetype_identifier>prasle1</motetype_identifier>
    </mote>
    <mote>
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>60.0</x>
        <y>100.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiMoteID
        <id>2</id>
      </interface_config>
      <motetype_identifier>prasle1</motetype_identifier>
    </mote>
    <mote>
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>100.0</x>
        <y>100.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiMoteID
        <id>3</id>
      </interface_config>
      <motetype_identifier>prasle1</motetype_identifier>
    </mote>
    <mote>
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>140.0</x>
        <y>100.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiMoteID
        <id>4</id>
      </interface_config>
      <motetype_identifier>prasle1</motetype_identifier>
    </mote>
    <mote>
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>180.0</x>
        <y>100.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiMoteID
        <id>5</id>
      </interface_config>
      <motetype_identifier>prasle1</motetype_identifier>
    </mote>
    <mote>
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>220.0</x>
        <y>100.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiMoteID
        <id>6</id>
      </interface_config>
      <motetype_identifier>prasle1</motetype_identifier>
    </mote>
    <mote>
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>260.0</x>
        <y>100.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiMoteID
        <id>7</id>
      </interface_config>
      <motetype_identifier>prasle1</motetype_identifier>
    </mote>
    <mote>
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>300.0</x>
        <y>100.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiMoteID
        <id>8</id>
      </interface_config>
      <motetype_identifier>prasle1</motetype_identifier>
    </mote>
    <mote>
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>340.0</x>
        <y>100.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiMoteID
        <id>9</id>
      </interface_config>
      <motetype_identifier>prasle1</motetype_identifier>
    </mote>
    <mote>
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>380.0</x>
        <y>100.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiMoteID
        <id>10</id>
      </interface_config>
      <motetype_identifier>prasle1</motetype_identifier>
    </mote>
  </simulation>
  <plugin>
    org.contikios.cooja.plugins.Visualizer
    <plugin_config>
      <moterelations>true</moterelations>
      <skin>org.contikios.cooja.plugins.skins.IDVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.GridVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.TrafficVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.UDGMVisualizerSkin</skin>
      <viewport>0.5 0.0 0.0 0.5 0.0 0.0</viewport>
    </plugin_config>
    <bounds x="1" y="1" height="400" width="400" z="2"/>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.LogListener
    <plugin_config>
      <filter />
      <formatted_time />
      <coloring />
    </plugin_config>
    <bounds x="400" y="1" height="400" width="800" z="1"/>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.TimeLine
    <plugin_config>
      <mote>0</mote>
      <showRadioRXTX />
      <showRadioHW />
      <showLEDs />
      <zoomfactor>500.0</zoomfactor>
    </plugin_config>
    <bounds x="0" y="400" height="200" width="1200" z="0"/>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.ScriptRunner
    <plugin_config>
      <script>
/* PRASLE Leader Election - Leader Crash Scenario Script */

// Read duration from environment or use default (5 minutes = 300000ms)
var duration = java.lang.System.getenv("COOJA_TIMEOUT");
if (duration == null) {
  duration = 300000;
} else {
  duration = parseInt(duration);
}

// Read crash time from environment or use default (60 seconds = 60000ms)
var crashTime = java.lang.System.getenv("CRASH_TIME");
if (crashTime == null) {
  crashTime = 60000;  // Default: crash after 60 seconds
} else {
  crashTime = parseInt(crashTime);
}

java.lang.System.out.println("===== LEADER CRASH SCENARIO STARTED =====");
java.lang.System.out.println("Duration: " + duration + "ms");
java.lang.System.out.println("Crash time: " + crashTime + "ms");
java.lang.System.out.println("Motes: " + sim.getMotesCount());

// Schedule leader crash and simulation end
GENERATE_MSG(crashTime, "crash_leader");
GENERATE_MSG(duration, "sim_end");

var leaderCrashed = false;

/* Listen to all mote output and forward to stdout */
var msgCount = 0;
while(true) {
  YIELD();

  msgCount++;

  // Check for leader crash event
  if (msg.equals("crash_leader") &amp;&amp; !leaderCrashed) {
    var moteCount = sim.getMotesCount();
    var leaderMote = sim.getMoteWithID(moteCount);  // Highest ID = leader

    if (leaderMote != null) {
      java.lang.System.out.println("===== CRASHING LEADER NODE " + moteCount + " at " + time + "ms =====");
      sim.removeMote(leaderMote);
      leaderCrashed = true;
      java.lang.System.out.println("===== LEADER NODE REMOVED - RE-ELECTION SHOULD START =====");
    } else {
      java.lang.System.out.println("WARNING: Leader mote not found!");
    }
  }

  // Check for simulation end
  if (msg.equals("sim_end")) {
    java.lang.System.out.println("===== SIMULATION END =====");
    java.lang.System.out.println("Messages received: " + msgCount);
    java.lang.System.out.println("Leader crashed: " + leaderCrashed);
    log.testOK();
  }

  // Forward mote output to stdout
  java.lang.System.out.println(time + " " + id + " " + msg);
}
      </script>
      <active>true</active>
    </plugin_config>
    <bounds x="800" y="100" height="400" width="600" z="3"/>
  </plugin>
</simconf>
