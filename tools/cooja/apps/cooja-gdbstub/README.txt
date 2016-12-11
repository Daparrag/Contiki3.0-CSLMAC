Name: COOJA GDBstub
Contact: Moritz "Morty" Strübe <morty@cs.fau.de>
License: BSD http://www.opensource.org/licenses/bsd-license.php
Intended platforms: Tmote Sky
Tested platforms: Tmote Sky
Description:
This GDBstub allows to connect GDB to COOJA. Consequently all GUIs 
built on top of GDB (e.g Eclipse) can connect to COOJA.

Warning:
======= 
GDB 3.2.3 seems broken. Tested with GDB 4.4.3 and Eclipse Helios + CDT 

Install:
=======
Copy or checkout to <COOJA>/apps/<foldename of your choosing>
run ant in that folder
in Cooja install the plugin:
Settings->ĆOOJA projects->search folder and click to make it green 
-> Save as Default   
 

Starting in COOJA:
=================
When you right-click a node in the Simulation Visualizer the GDB-Plugin 
should be listed in the "Open mote plugin for Sky" menu.
It also shows you the port you need to connect to.

Debugging with GDB:
==================
start up msp430-gdb, and type
> file <file.sky>
> target remote localhost:2000
Now you should be able to debug using GDB.

Debugging in Eclipse Helios with CDT:
====================================
Run -> Debug Configurations... -> C/C++ Application -> New

At the very bottom: "Using .... Select other:" 
	-> Use configation specific settings
	-> Remote System Process Launcher
	-> OK

Main
	C/C++ Application: .sky you want to debug
	Disable Auto-Build

Debugger
	Main
		GDB debugger: <Path of msp430-gdb version 4.4.3>
	Connection
		Type: TCP
		Host: localhost
		Port: 2000 or whatever the gdb-stub in COOJA shows you




	

