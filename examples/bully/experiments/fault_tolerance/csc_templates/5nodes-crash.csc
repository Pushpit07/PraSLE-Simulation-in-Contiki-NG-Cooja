<?xml version="1.0" encoding="UTF-8"?>
<simconf>
  <simulation>
    <title>Bully Leader Election Algorithm - 5 Nodes - Leader Crash Scenario</title>
    <randomseed>123456</randomseed>
    <motedelay_us>1000000</motedelay_us>
    <radiomedium>
      org.contikios.cooja.radiomediums.UDGM
      <transmitting_range>100.0</transmitting_range>
      <interference_range>100.0</interference_range>
      <success_ratio_tx>1.0</success_ratio_tx>
      <success_ratio_rx>1.0</success_ratio_rx>
    </radiomedium>
    <events>
      <logoutput>40000</logoutput>
    </events>
    <motetype>
      org.contikios.cooja.contikimote.ContikiMoteType
      <identifier>bully1</identifier>
      <description>Bully Node Type</description>
      <source>[CONTIKI_DIR]/examples/bully/bully-node.c</source>
      <commands>$(MAKE) -j$(CPUS) bully-node.cooja TARGET=cooja</commands>
      <moteinterface>org.contikios.cooja.interfaces.Position</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.Battery</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiVib</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiMoteID</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiRS232</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiBeeper</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.RimeAddress</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiIPAddress</moteinterface>
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

    <!-- Node 1 - Position: (20, 20) -->
    <mote>
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>20.0</x>
        <y>20.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiMoteID
        <id>1</id>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiRadio
        <bitrate>250.0</bitrate>
      </interface_config>
      <motetype_identifier>bully1</motetype_identifier>
    </mote>

    <!-- Node 2 - Position: (60, 20) -->
    <mote>
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>60.0</x>
        <y>20.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiMoteID
        <id>2</id>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiRadio
        <bitrate>250.0</bitrate>
      </interface_config>
      <motetype_identifier>bully1</motetype_identifier>
    </mote>

    <!-- Node 3 - Position: (100, 20) -->
    <mote>
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>100.0</x>
        <y>20.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiMoteID
        <id>3</id>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiRadio
        <bitrate>250.0</bitrate>
      </interface_config>
      <motetype_identifier>bully1</motetype_identifier>
    </mote>

    <!-- Node 4 - Position: (20, 60) -->
    <mote>
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>20.0</x>
        <y>60.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiMoteID
        <id>4</id>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiRadio
        <bitrate>250.0</bitrate>
      </interface_config>
      <motetype_identifier>bully1</motetype_identifier>
    </mote>

    <!-- Node 5 - Position: (60, 60) -->
    <mote>
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>60.0</x>
        <y>60.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiMoteID
        <id>5</id>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiRadio
        <bitrate>250.0</bitrate>
      </interface_config>
      <motetype_identifier>bully1</motetype_identifier>
    </mote>

  </simulation>
  <plugin>
    org.contikios.cooja.plugins.SimControl
    <width>280</width>
    <z>0</z>
    <height>160</height>
    <location_x>400</location_x>
    <location_y>0</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.Visualizer
    <plugin_config>
      <moterelations>true</moterelations>
      <skin>org.contikios.cooja.plugins.skins.IDVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.UDGMVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.MoteTypeVisualizerSkin</skin>
      <viewport>0.9090909090909091 0.0 0.0 0.9090909090909091 25.454545454545438 14.090909090909092</viewport>
    </plugin_config>
    <width>400</width>
    <z>3</z>
    <height>400</height>
    <location_x>1</location_x>
    <location_y>1</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.LogListener
    <plugin_config>
      <filter />
      <formatted_time />
      <coloring />
    </plugin_config>
    <width>1184</width>
    <z>2</z>
    <height>240</height>
    <location_x>402</location_x>
    <location_y>162</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.TimeLine
    <plugin_config>
      <mote>0</mote>
      <mote>1</mote>
      <mote>2</mote>
      <mote>3</mote>
      <mote>4</mote>
      <showRadioRXTX />
      <showRadioHW />
      <showLEDs />
      <zoomfactor>500.0</zoomfactor>
    </plugin_config>
    <width>1584</width>
    <z>1</z>
    <height>166</height>
    <location_x>0</location_x>
    <location_y>403</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.ScriptRunner
    <plugin_config>
      <script>/* Bully Algorithm Leader Crash Scenario Script */

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
}</script>
      <active>true</active>
    </plugin_config>
    <width>600</width>
    <z>3</z>
    <height>700</height>
    <location_x>700</location_x>
    <location_y>0</location_y>
  </plugin>
</simconf>
