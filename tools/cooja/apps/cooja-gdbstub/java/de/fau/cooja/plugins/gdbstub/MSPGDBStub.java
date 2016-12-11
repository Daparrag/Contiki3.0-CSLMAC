/**
 * Copyright (c) 2007, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file was part of MSPSim.
 * This file is part of COOJA.
 *
 * $Id: $
 *
 * -----------------------------------------------------------------
 *
 * GDBStub
 *
 * Author  : Joakim Eriksson
 * Created : 31 mar 2008
 * Updated : $Date:$
 *           $Revision:$
 *
 * \author Joakim Eriksson           
 * \author Moritz "Morty" Struebe <Moritz.Struebe@informatik.uni-erlangen.de>
 */
package de.fau.cooja.plugins.gdbstub;


import java.awt.GridLayout;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.Collection;
import java.util.Observable;
import java.util.Observer;
import java.util.Vector;

import javax.swing.JLabel;

import org.apache.log4j.Level;
import org.apache.log4j.Logger;
import org.jdom.Element;
import org.contikios.cooja.ClassDescription;
import org.contikios.cooja.Cooja;
import org.contikios.cooja.Mote;
import org.contikios.cooja.MotePlugin;
import org.contikios.cooja.PluginType;
import org.contikios.cooja.Simulation;
import org.contikios.cooja.SupportedArguments;
import org.contikios.cooja.VisPlugin;
import org.contikios.cooja.mspmote.MspMote;

import se.sics.mspsim.core.Memory.AccessMode;
import se.sics.mspsim.core.Memory.AccessType;
import se.sics.mspsim.core.MemoryMonitor;
import se.sics.mspsim.core.EmulationException;
import se.sics.mspsim.core.MSP430Core;
import se.sics.mspsim.util.Utils;

@ClassDescription("Msp GDBStub")
@PluginType(PluginType.MOTE_PLUGIN)
@SupportedArguments(motes = {MspMote.class})
public class MSPGDBStub extends VisPlugin implements Runnable, MotePlugin {
	/**
	 * 
	 */
	private static final long serialVersionUID = 1L;
	private static Logger logger = Logger.getLogger(MSPGDBStub.class);
	private Simulation simulation;
	private Observer simObserver;
	private MspMote mspMote;
	private MSP430Core cpu;
	private boolean stoppedInt = false;
	private Integer port = 0;
	private JLabel info;
	private Thread server_thread;
	private boolean stopThread = false;
	private Vector<Integer> mBps = new Vector<Integer>();
	
	private MemoryMonitor mBp = new MemoryMonitor.Adapter() {															
		@Override
		public void notifyReadAfter(int addr, AccessMode mode, AccessType type) {
			if (mspMote.getSimulation().isRunning()) {
				/* Stop simulation immediately */
				mspMote.stopNextInstruction();
			}
			
		}

		@Override
		public void notifyWriteAfter(int dstAddress, int data, AccessMode mode) {
			if (mspMote.getSimulation().isRunning()) {
				/* Stop simulation immediately */
				mspMote.stopNextInstruction();
			}
			
		}
	};
	
	private Vector<Integer> mBpReads = new Vector<Integer>();
	private MemoryMonitor mBpRead = new MemoryMonitor.Adapter() {															
		@Override
		public void notifyReadAfter(int addr, AccessMode mode, AccessType type) {
			if (mspMote.getSimulation().isRunning()) {
				/* Stop simulation immediately */
				mspMote.stopNextInstruction();
			}
			
		}
	};
	
	private Vector<Integer> mBpWrites = new Vector<Integer>();
	private MemoryMonitor mBpWrite = new MemoryMonitor.Adapter() {															
		@Override
		public void notifyWriteAfter(int dstAddress, int data, AccessMode mode) {
			if (mspMote.getSimulation().isRunning()) {
				/* Stop simulation immediately */
				mspMote.stopNextInstruction();
			}
			
		}
	};
	

	/**
	 * GDB-Stub for MSP430
	 * 
	 * @param mote
	 *            MSP Mote
	 * @param simulationToVisualize
	 *            Simulation
	 * @param cooja
	 *            Simulator
	 */
	public MSPGDBStub(Mote mote, Simulation simulationToVisualize, Cooja cooja) {
		super("Msp GDBStub", cooja);
		
		logger.setLevel(Level.INFO);
		
		this.mspMote = (MspMote) mote;
		this.cpu = this.mspMote.getCPU();
		simulation = simulationToVisualize;
		
		getContentPane().setLayout(new GridLayout(0,1));

		/* Some output */
		add(new JLabel("Unstable - do not use together with CodeWatcher"));
		info = new JLabel("");
		add(info);



		/* Observe when simulation starts/stops */
		simulation.addObserver(simObserver = new Observer() {
			public void update(Observable obs, Object obj) {
				if (!simulation.isRunning()) {
					try {
						// SIGTSTP
						if (stoppedInt) {
							sendResponse("S" + Utils.hex8(2));
							stoppedInt = false;
						} else {
							sendResponse("S" + Utils.hex8(18));
						}
					} catch (IOException e) {
					}
					;
				} else {

				}
			}
		});


		setSize(350, 80);

	}

	public void startPlugin() {
		if(port == 0){
			port = 2000;
			for (int ctr = 0; ctr < 10; ctr++) {
				if (setupServer(port))
					break;
				port++;
	
			}
		} else {
			setupServer(port);
		}
	}
	
	
	public void closePlugin() {
		// Shutdown server?!
		simulation.deleteObserver(simObserver);

		stopThread = true;
		try{
			serverSocket.close();
		} catch (Exception e) {
			// TODO: handle exception
		}
		
		
	}
	
	

	// ------------------- Actual server

	private final static String OK = "OK";

	private ServerSocket serverSocket;
	private OutputStream output;

	public boolean setupServer(int port) {
		try {
			logger.info("Opening gdb server on port " + port);
			info.setText("Opening gdb server on port " + port);
			serverSocket = new ServerSocket(port, 1);
			server_thread = new Thread(this);
			server_thread.start();
		} catch (IOException e) {
			e.printStackTrace();
			info.setText("Failed to open gdb server on port " + port);
			return false;
		}
		logger.info("GDB server running on port " + port);
		info.setText("GDB server running on port " + port);
		return true;
	}

	int[] buffer = new int[256];
	int len;

	public void run() {
		while (!stopThread) {
			try {
				Socket s = serverSocket.accept();
				//turn off delay!
				s.setTcpNoDelay(true);
				DataInputStream input = new DataInputStream(s.getInputStream());
				output = s.getOutputStream();

				String cmd = "";
				boolean readCmd = false;
				int cs = 0;
				int c;
				while (s != null && ((c = input.read()) != -1)) {
					/*
					 * logger.info("GDBStub: Read  " + c + " => " + (char) c);
					 */
					if (readCmd) {
						if (cs > 0) {
							cs--;
							if (cs == 0) {
								readCmd = false;
								/* ack the message */
								output.write('+');
								handleCmd(cmd, buffer, len);
								output.flush();
								cmd = "";
								len = 0;
							}
						} else if (c == '#') {
							cs = 2;
						} else {
							cmd += (char) c;
							buffer[len++] = (c & 0xff);
						}
					} else if (c == '$') {
						readCmd = true;
					} else if (c == 0x03) {
						stoppedInt = true;
						simulation.stopSimulation();
					}

				}
			} catch (IOException e) {
				e.printStackTrace();
			} catch (EmulationException e) {
				e.printStackTrace();
			}
		}
		try{
			serverSocket.close();
		} catch (Exception e) {
			// TODO: handle exception
		}
		logger.debug("Thread stoped");
	}

	private void handleCmd(String cmd, int[] cmdBytes, int cmdLen)
			throws IOException, EmulationException {
		logger.debug("cmd: " + cmd);
		char c = cmd.charAt(0);

		boolean remove = false;
		switch (c) {
		case '?':
			// Must be stoped, as this is expected
			simulation.stopSimulation();
			sendResponse("S" + Utils.hex8(5));
			break;
		case 'c':
		case 'C':
			simulation.startSimulation();
			break;
		case 'g':
			readRegisters();
			break;
		case 'k': // kill
			sendResponse(OK);
			break;
		case 'H':
			sendResponse(OK);
			break;
		case 'q':
			//Theradextainfo: Output Text-String
			if ("qC".equals(cmd)) {
				sendResponse("QC1");
			} else if ("qOffsets".equals(cmd)) {
				sendResponse("Text=0;Data=0;Bss=0");
			} else if ("qfThreadInfo".equals(cmd)) {
				sendResponse("m 01");
			} else if ("qsThreadInfo".equals(cmd)) {
				sendResponse("l");
			} else if ("qSymbol::".equals(cmd)) {
				sendResponse(OK);
				// } else if ("qThreadExtraInfo,1".equals(cmd)){
				// sendResponse(stringToHex("Stoped"));
			} else if ("qSupported".equals(cmd)) {
				sendResponse("PacketSize=128");
			} else {
				logger.info("Query unknown: " + cmd);
				sendResponse("");
			}

			break;
		case 'S':
		case 's':
			try {
				mspMote.getCPU().stepInstructions(1);
				sendResponse("S" + Utils.hex8(5));
			} catch (EmulationException ex) {
				logger.fatal("CMD: " + cmd + " Error: ", ex);
				sendResponse("E02");
			}
			
			break;
		case 'T':
			int t = Integer.parseInt(cmd.substring(1));
			if(t == 1){
				sendResponse("OK");
			} else {
				sendResponse("E01");
			}
			break;
		case 'm':
		case 'M':
		case 'X': {
			String cmd2 = cmd.substring(1);
			String wdata[] = cmd2.split(":");
			int cPos = cmd.indexOf(':');
			if (cPos > 0) {
				/* only until length in first part */
				cmd2 = wdata[0];
			}
			String parts[] = cmd2.split(",");
			int addr = Integer.decode("0x" + parts[0]);
			int len = Integer.decode("0x" + parts[1]);
			String data = "";
			if (c == 'm') {
				logger.debug("Returning memory from: " + addr + " len = " + len);
				/* This might be wrong - which is the correct byte order? */
				try {
					for (int i = 0; i < len; i++) {
						data += Utils.hex8(cpu.memory[addr++] & 0xff);
					}
					
					sendResponse(data);
				} catch (Exception ex) {
					logger.fatal("CMD: " + cmd +" ", ex);
					sendResponse("E03");
				}
				
			} else {
				int i;
				logger.info("Writing to memory at: " + addr + " len = " + len
						+ " with: " + ((wdata.length > 1) ? wdata[1] : ""));
				cPos++;
				for (i = 0; i < len; i++) {
					/*System.out.println("Writing: " + cmdBytes[cPos] + " to "
							+ addr + " cpos=" + cPos);*/
					cpu.memory[addr++] = cmdBytes[cPos];
					cPos++;
				}
				if(i != len){
					sendResponse("E 01");
				} else {
					sendResponse(OK);
				}
			}
			break;
		}
		
		
		case 'z':
			remove = true;
		case 'Z': { // Add breakpoint
			
			String[] tokens = cmd.split(",");
			Integer addr = new Integer(Integer.parseInt(tokens[1], 16));
			logger.debug("ADDR: " + addr );
			MemoryMonitor bp = null;
			Vector <Integer> bpv = null;
			switch (cmd.charAt(1)) {
			case '0': //This should be a memory breakpoint - but we don't care
			case '1':
				bp = mBp;
				bpv = mBps;
				break;
			case '2': 
				bp = mBpWrite;
				bpv = mBpWrites;
				break;
			case '3':
				bp = mBpRead;
				bpv = mBpReads;
				break;
			case '4':
				//Here we can use the default breakpoint
				bp = mBp;
				bpv = mBps;
				break;
			default: // Not supported
				
			}
			if(bp != null){
				if (!remove) {
					//Add breakpoint to out list
					bpv.add(addr);
					cpu.addWatchPoint(addr, bp);

				} else {
					if(bpv.remove(addr)){
						cpu.removeWatchPoint(addr, bp);
					}
				}
				sendResponse("OK");
			} else {
				sendResponse("");
			}
			break;
		}

		default:
			logger.info("Command unknown: " + cmd);
			sendResponse("");
		}
	}

	private void readRegisters() throws IOException {
		String regs = "";
		for (int i = 0; i < 16; i++) {
			regs += Utils.hex8(cpu.reg[i] & 0xff) + Utils.hex8(cpu.reg[i] >> 8);
		}
		sendResponse(regs);
	}

	public static String stringToHex(String base) {
		StringBuffer buffer = new StringBuffer();
		int intValue;
		for (int x = 0; x < base.length(); x++) {
			int cursor = 0;
			intValue = base.charAt(x);
			String binaryChar = new String(Integer.toBinaryString(base
					.charAt(x)));
			for (int i = 0; i < binaryChar.length(); i++) {
				if (binaryChar.charAt(i) == '1') {
					cursor += 1;
				}
			}
			if ((cursor % 2) > 0) {
				intValue += 128;
			}
			buffer.append(Integer.toHexString(intValue));
		}
		return buffer.toString();
	}

	public void sendResponse(String resp) throws IOException {
		if (output == null)
			return;

		logger.debug("Resp: -" + resp + "-");
		output.write('$');

		int cs = 0;
		if (resp != null) {
			for (int i = 0; i < resp.length(); i++) {
				output.write((char) resp.charAt(i));
				cs += resp.charAt(i);
			}
		}
		output.write('#');

		int c = (cs & 0xff) >> 4;
		if (c < 10) {
			c = c + '0';
		} else {
			c = c - 10 + 'a';
		}
		output.write((char) c);

		c = cs & 15;
		if (c < 10) {
			c = c + '0';
		} else {
			c = c - 10 + 'a';
		}
		output.write((char) c);

	}

	public boolean setConfigXML(Collection<Element> configXML,
			boolean visAvailable) {
		for (Element element : configXML) {
			if (element.getName().equals("port")) {
				port = Integer.parseInt(element.getText());
				logger.debug("Config-Port: " + port);
			}
		}
		return true;
	}

	public Collection<Element> getConfigXML() {
		Vector<Element> config = new Vector<Element>();
		Element element;

		element = new Element("port");

		element.setText(port.toString());
		config.add(element);
		return config;
	}
	
	  

	/**
	 * @return MSP mote
	 */
	public MspMote getMote() {
		return mspMote;
	}
}

