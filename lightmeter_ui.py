import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import serial
import serial.tools.list_ports
import threading
import time
import re
import random  # For demo mode
import math

class LightMeterUI:
    def __init__(self, root):
        self.root = root
        self.root.title("4x5 Lightmeter Simulator")
        self.root.geometry("800x600")
        self.root.resizable(True, True)
        
        # Serial connection
        self.serial = None
        self.serial_thread = None
        self.thread_running = False
        
        # Device settings
        self.iso = 100
        self.metering_type = "center"
        self.calibration = 128.0
        self.shutter_speed = "1/125"
        self.ev_value = "N/A"
        self.demo_mode = False
        self.light_values = [[random.uniform(10, 500) for _ in range(4)] for _ in range(5)]  # Random light values for demo
        
        # UI/Menu state
        self.current_screen = "main"  # main, menu, config_iso, config_type, config_calibration
        self.menu_options = ["ISO", "Metering Type", "Calibration", "Exit Menu"]
        self.menu_index = 0
        self.config_timeout = None  # Timer for returning to main screen
        self.config_timeout_ms = 5000  # Return to main screen after 5 seconds of inactivity
        
        # Create widgets
        self.create_frames()
        self.create_display()
        self.create_buttons()
        self.create_debug_area()
        self.create_serial_controls()
        
        # Start with demo mode enabled
        self.toggle_demo_mode()
        
        # Update display initially
        self.update_display()

    def create_frames(self):
        # Main frames
        self.display_frame = ttk.Frame(self.root, padding="10")
        self.display_frame.pack(fill=tk.BOTH, expand=True)
        
        self.button_frame = ttk.Frame(self.root, padding="10")
        self.button_frame.pack(fill=tk.X)
        
        self.debug_frame = ttk.Frame(self.root, padding="10")
        self.debug_frame.pack(fill=tk.BOTH, expand=True)
        
        # Separator
        ttk.Separator(self.root, orient=tk.HORIZONTAL).pack(fill=tk.X, padx=10)

    def create_display(self):
        # E-ink display simulation (small screen)
        self.display_canvas = tk.Canvas(self.display_frame, bg="#E5E5E5", height=250, bd=2, relief=tk.SUNKEN)
        self.display_canvas.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Initialize screen elements (will be populated in update_display)
        self.screen_elements = {}
        
        # Debug: light matrix visualization (not visible on e-ink display)
        self.debug_matrix_frame = ttk.LabelFrame(self.display_frame, text="Light Sensor Matrix (Debug Only)")
        self.debug_matrix_frame.pack(fill=tk.X, pady=10)
        
        self.debug_matrix_canvas = tk.Canvas(self.debug_matrix_frame, bg="white", height=100)
        self.debug_matrix_canvas.pack(fill=tk.X, expand=True, padx=5, pady=5)
        
        self.light_matrix = []
        for row in range(5):
            matrix_row = []
            for col in range(4):
                x = 100 + col * 40
                y = 10 + row * 20
                rect = self.debug_matrix_canvas.create_rectangle(x, y, x+30, y+15, fill="white", outline="black", tags=f"led_{row}_{col}")
                matrix_row.append(rect)
            self.light_matrix.append(matrix_row)
            
        # Highlight spot meter LEDs
        self.debug_matrix_canvas.create_text(10, 50, text="Spot LEDs:", anchor="w")
        self.debug_matrix_canvas.create_text(70, 50, text="10, 11", anchor="w", fill="red")

    def create_buttons(self):
        # Physical buttons simulation
        btn_style = {"width": 10, "padding": 10}
        
        self.measure_btn = ttk.Button(self.button_frame, text="MEASURE", command=self.measure_click, **btn_style)
        self.measure_btn.pack(side=tk.LEFT, padx=10)
        
        self.up_btn = ttk.Button(self.button_frame, text="UP", command=self.button_up, **btn_style)
        self.up_btn.pack(side=tk.LEFT, padx=10)
        
        self.down_btn = ttk.Button(self.button_frame, text="DOWN", command=self.button_down, **btn_style)
        self.down_btn.pack(side=tk.LEFT, padx=10)
        
        # Long press simulation for menu
        self.measure_btn.bind("<ButtonPress-1>", self.measure_press)
        self.measure_btn.bind("<ButtonRelease-1>", self.measure_release)
        self.press_time = 0
        
        # Demo mode toggle
        self.demo_btn = ttk.Button(self.button_frame, text="Toggle Demo", command=self.toggle_demo_mode, **btn_style)
        self.demo_btn.pack(side=tk.RIGHT, padx=10)
        
        # Randomize values (for demo)
        self.random_btn = ttk.Button(self.button_frame, text="Randomize", command=self.randomize_values, **btn_style)
        self.random_btn.pack(side=tk.RIGHT, padx=10)

    def create_debug_area(self):
        # Debug and UART console
        ttk.Label(self.debug_frame, text="UART Console:").pack(anchor=tk.W)
        
        self.console = scrolledtext.ScrolledText(self.debug_frame, height=8)
        self.console.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Manual command entry
        cmd_frame = ttk.Frame(self.debug_frame)
        cmd_frame.pack(fill=tk.X, pady=5)
        
        ttk.Label(cmd_frame, text="Command:").pack(side=tk.LEFT)
        self.cmd_entry = ttk.Entry(cmd_frame, width=50)
        self.cmd_entry.pack(side=tk.LEFT, padx=5, fill=tk.X, expand=True)
        self.cmd_entry.bind("<Return>", self.send_command)
        
        send_btn = ttk.Button(cmd_frame, text="Send", command=lambda: self.send_command(None))
        send_btn.pack(side=tk.LEFT, padx=5)

    def create_serial_controls(self):
        # Serial connection controls
        serial_frame = ttk.LabelFrame(self.debug_frame, text="Serial Connection")
        serial_frame.pack(fill=tk.X, pady=5)
        
        # Port selection
        port_frame = ttk.Frame(serial_frame)
        port_frame.pack(fill=tk.X, pady=5)
        
        ttk.Label(port_frame, text="Port:").pack(side=tk.LEFT)
        self.port_combo = ttk.Combobox(port_frame, width=30)
        self.port_combo.pack(side=tk.LEFT, padx=5)
        
        refresh_btn = ttk.Button(port_frame, text="Refresh", command=self.refresh_ports)
        refresh_btn.pack(side=tk.LEFT, padx=5)
        
        # Connect button
        self.connect_btn = ttk.Button(serial_frame, text="Connect", command=self.toggle_connection)
        self.connect_btn.pack(side=tk.LEFT, padx=5, pady=5)
        
        # Populate ports
        self.refresh_ports()

    def refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports:
            self.port_combo.current(0)

    def toggle_connection(self):
        if self.serial is None:
            try:
                port = self.port_combo.get()
                self.serial = serial.Serial(port, 115200, timeout=1)
                self.connect_btn.config(text="Disconnect")
                self.log("Connected to " + port)
                
                # Start reading thread
                self.thread_running = True
                self.serial_thread = threading.Thread(target=self.read_serial, daemon=True)
                self.serial_thread.start()
                
                # Disable demo mode when connected
                self.demo_mode = False
            except Exception as e:
                self.log(f"Error connecting: {str(e)}")
                self.serial = None
        else:
            self.thread_running = False
            if self.serial_thread:
                self.serial_thread.join(timeout=1)
            
            if self.serial:
                self.serial.close()
                self.serial = None
            
            self.connect_btn.config(text="Connect")
            self.log("Disconnected")

    def read_serial(self):
        while self.thread_running and self.serial:
            try:
                if self.serial.in_waiting:
                    data = self.serial.readline().decode('utf-8', errors='replace')
                    self.log(data.strip())
                    self.parse_response(data)
                else:
                    time.sleep(0.1)
            except Exception as e:
                self.log(f"Serial error: {str(e)}")
                break
        
        # Clean up on thread exit
        if self.serial:
            self.serial.close()
            self.serial = None
        self.root.after(0, lambda: self.connect_btn.config(text="Connect"))

    def send_command(self, event):
        cmd = self.cmd_entry.get().strip()
        if not cmd:
            return
        
        self.log(f"> {cmd}")
        self.cmd_entry.delete(0, tk.END)
        
        if self.serial and self.serial.is_open:
            try:
                self.serial.write((cmd + '\r\n').encode('utf-8'))
            except Exception as e:
                self.log(f"Error sending command: {str(e)}")
        else:
            # Process commands in demo mode
            self.process_demo_command(cmd)

    def process_demo_command(self, cmd):
        if cmd.startswith("config iso "):
            try:
                self.iso = int(cmd.split()[-1])
                self.log(f"ISO set to {self.iso}")
                self.update_display()
            except ValueError:
                self.log("Invalid ISO value")
        
        elif cmd.startswith("config type "):
            type_val = cmd.split()[-1].lower()
            valid_types = {
                "center": "center", 
                "central": "center", 
                "center-weighted": "center",
                "matrix": "matrix", 
                "evaluative": "matrix",
                "spot": "spot",
                "highlight": "highlight", 
                "highlights": "highlight"
            }
            
            if type_val in valid_types:
                self.metering_type = valid_types[type_val]
                self.log(f"Metering type set to {self.metering_type}")
                self.update_display()
            else:
                self.log("Invalid metering type")
        
        elif cmd.startswith("config calibration "):
            try:
                self.calibration = float(cmd.split()[-1])
                self.log(f"Calibration set to {self.calibration}")
                self.update_display()
            except ValueError:
                self.log("Invalid calibration value")
        
        elif cmd == "start measure":
            self.take_measurement()
        
        elif cmd == "help":
            help_text = """
Available commands:
config iso <value> - Set ISO value
config type <mode> - Set metering type (center, matrix, spot, highlight)
config calibration <value> - Set calibration factor
start measure - Start light measurement
help - Show this help
reset - Reset device
"""
            self.log(help_text)
        
        elif cmd == "reset":
            self.iso = 100
            self.metering_type = "center"
            self.calibration = 128.0
            self.log("Device reset to defaults")
            self.update_display()
        
        else:
            self.log("Unknown command")

    def measure_press(self, event):
        self.press_time = time.time()

    def measure_release(self, event):
        # This is for handling the simulated button UI events
        # The actual click handler is separate to avoid double triggering
        pass
    
    def measure_click(self):
        # Check if it was a long press (>1 second)
        if time.time() - self.press_time > 1:
            # Long press - toggle menu/main
            if self.current_screen == "main":
                self.current_screen = "menu"
                self.menu_index = 0
            else:
                self.current_screen = "main"
            self.update_display()
        else:
            # Short press - depends on current screen
            if self.current_screen == "main":
                # Take a measurement
                self.take_measurement()
            elif self.current_screen == "menu":
                # Select menu option
                if self.menu_index == 0:
                    self.current_screen = "config_iso"
                elif self.menu_index == 1:
                    self.current_screen = "config_type"
                elif self.menu_index == 2:
                    self.current_screen = "config_calibration"
                elif self.menu_index == 3:
                    self.current_screen = "main"
                self.update_display()
                
                # Set timeout to return to main screen
                self.reset_config_timeout()
            else:
                # From config screens, return to main
                self.current_screen = "main"
                self.update_display()

    def reset_config_timeout(self):
        # Cancel existing timeout if any
        if self.config_timeout:
            self.root.after_cancel(self.config_timeout)
            
        # Only set timeout for config screens
        if self.current_screen in ["config_iso", "config_type", "config_calibration"]:
            self.config_timeout = self.root.after(self.config_timeout_ms, self.timeout_to_main)
    
    def timeout_to_main(self):
        # Return to main screen after timeout
        if self.current_screen != "main":
            self.current_screen = "main"
            self.update_display()
            self.log("Returned to main screen due to inactivity")

    def button_up(self):
        # Reset config timeout if in a config screen
        self.reset_config_timeout()
        
        if self.current_screen == "main":
            # In main screen, do nothing
            pass
        elif self.current_screen == "menu":
            # Navigate menu
            self.menu_index = (self.menu_index - 1) % len(self.menu_options)
            self.update_display()
        elif self.current_screen == "config_iso":
            # Change ISO
            iso_values = [50, 100, 200, 400, 800, 1600, 3200, 6400]
            try:
                idx = iso_values.index(self.iso)
                self.iso = iso_values[(idx + 1) % len(iso_values)]
                # Send command to ESP32
                self.send_config_command(f"config iso {self.iso}")
            except ValueError:
                self.iso = 100
                self.send_config_command(f"config iso {self.iso}")
            self.update_display()
        elif self.current_screen == "config_type":
            # Change metering type
            types = ["center", "matrix", "spot", "highlight"]
            try:
                idx = types.index(self.metering_type)
                self.metering_type = types[(idx + 1) % len(types)]
                # Send command to ESP32
                self.send_config_command(f"config type {self.metering_type}")
            except ValueError:
                self.metering_type = "center"
                self.send_config_command(f"config type {self.metering_type}")
            self.update_display()
            
            # Update debug light matrix
            self.update_light_matrix()
        elif self.current_screen == "config_calibration":
            # Change calibration - increment by 10%
            self.calibration = round(self.calibration * 1.1, 1)
            # Send command to ESP32
            self.send_config_command(f"config calibration {self.calibration}")
            self.update_display()

    def button_down(self):
        # Reset config timeout if in a config screen
        self.reset_config_timeout()
        
        if self.current_screen == "main":
            # In main screen, do nothing
            pass
        elif self.current_screen == "menu":
            # Navigate menu
            self.menu_index = (self.menu_index + 1) % len(self.menu_options)
            self.update_display()
        elif self.current_screen == "config_iso":
            # Change ISO
            iso_values = [50, 100, 200, 400, 800, 1600, 3200, 6400]
            try:
                idx = iso_values.index(self.iso)
                self.iso = iso_values[(idx - 1) % len(iso_values)]
                # Send command to ESP32
                self.send_config_command(f"config iso {self.iso}")
            except ValueError:
                self.iso = 100
                self.send_config_command(f"config iso {self.iso}")
            self.update_display()
        elif self.current_screen == "config_type":
            # Change metering type
            types = ["center", "matrix", "spot", "highlight"]
            try:
                idx = types.index(self.metering_type)
                self.metering_type = types[(idx - 1) % len(types)]
                # Send command to ESP32
                self.send_config_command(f"config type {self.metering_type}")
            except ValueError:
                self.metering_type = "center"
                self.send_config_command(f"config type {self.metering_type}")
            self.update_display()
            
            # Update debug light matrix
            self.update_light_matrix()
        elif self.current_screen == "config_calibration":
            # Change calibration - decrement by 10%
            self.calibration = round(self.calibration / 1.1, 1)
            # Send command to ESP32
            self.send_config_command(f"config calibration {self.calibration}")
            self.update_display()
    
    def send_config_command(self, cmd):
        """Send configuration command to ESP32 or process in demo mode"""
        if self.serial and self.serial.is_open:
            try:
                self.serial.write((cmd + '\r\n').encode('utf-8'))
                self.log(f"> {cmd}")
            except Exception as e:
                self.log(f"Error sending command: {str(e)}")
        else:
            # In demo mode, just log the command
            self.log(f"> {cmd} (demo mode)")
            
    def measure_click(self):
        # Check if it was a long press (>1 second)
        if time.time() - self.press_time > 1:
            # Long press - toggle menu/main
            if self.current_screen == "main":
                self.current_screen = "menu"
                self.menu_index = 0
            else:
                # Return to main from any screen
                self.current_screen = "main"
            self.update_display()
        else:
            # Short press - depends on current screen
            if self.current_screen == "main":
                # Take a measurement
                self.take_measurement()
            elif self.current_screen == "menu":
                # Select menu option
                if self.menu_index == 0:
                    self.current_screen = "config_iso"
                elif self.menu_index == 1:
                    self.current_screen = "config_type"
                elif self.menu_index == 2:
                    self.current_screen = "config_calibration"
                elif self.menu_index == 3:
                    self.current_screen = "main"
                self.update_display()
                
                # Set timeout to return to main screen
                self.reset_config_timeout()
            else:
                # From config screens, return to main
                self.current_screen = "main"
                self.update_display()

    def take_measurement(self):
        if self.current_screen != "main":
            # Only take measurements from main screen
            return
            
        if self.serial and self.serial.is_open:
            try:
                self.serial.write("start measure\r\n".encode('utf-8'))
                self.log("> start measure")
            except Exception as e:
                self.log(f"Error sending command: {str(e)}")
        else:
            # Demo mode
            self.simulate_measurement()

    def simulate_measurement(self):
        # Simulate lightmeter response
        if self.demo_mode:
            # Generate simulated light values (more realistic)
            base_lux = random.uniform(50, 2000)  # Base light level
            variation = 0.3  # How much variation between sensors (30%)
            highlight_sensor = (random.randint(0, 4), random.randint(0, 3))  # Random highlight spot
            
            # Update light matrix with some variation
            for row in range(5):
                for col in range(4):
                    # More light in the highlight area
                    distance = ((row - highlight_sensor[0])**2 + (col - highlight_sensor[1])**2)**0.5
                    falloff = max(0.5, 1 - (distance/8))  # Light falloff
                    
                    if self.metering_type == "highlight" and row == highlight_sensor[0] and col == highlight_sensor[1]:
                        # Extra bright highlight
                        self.light_values[row][col] = base_lux * 3
                    else:
                        # Normal variation
                        self.light_values[row][col] = base_lux * falloff * random.uniform(1-variation, 1+variation)
            
            # Calculate EV based on metering type
            if self.metering_type == "center":
                # Center-weighted average
                center_weight = 0.6
                center_lux = (self.light_values[2][1] + self.light_values[2][2]) / 2
                peripheral_lux = (sum([sum(row) for row in self.light_values]) - self.light_values[2][1] - self.light_values[2][2]) / 18
                avg_lux = center_lux * center_weight + peripheral_lux * (1 - center_weight)
                
            elif self.metering_type == "matrix":
                # Matrix metering (average of all)
                valid_values = [val for row in self.light_values for val in row]
                avg_lux = sum(valid_values) / len(valid_values)
                
            elif self.metering_type == "spot":
                # Spot metering (center only)
                avg_lux = (self.light_values[2][1] + self.light_values[2][2]) / 2
                
            elif self.metering_type == "highlight":
                # Highlight metering (brightest patch)
                avg_lux = max([max(row) for row in self.light_values])
            
            # Calculate EV and shutter speed
            ev = max(-6.0, min(20.0, round(math.log2(avg_lux/2.5), 1)))
            t = 2**(-ev) * (100/self.iso) * self.calibration
            
            if t < 1:
                shutter = f"1/{int(round(1/t))}"
            else:
                shutter = f"{t:.1f} seconds"
            
            self.shutter_speed = shutter
            self.ev_value = f"{ev:.1f}"
            
            # Create simulated detailed measurements output
            header = "================= DETAILED MEASUREMENTS =================\n"
            header += "    | Column 1      | Column 2      | Column 3      | Column 4      |\n"
            header += "Row | ADC  V    Lux | ADC  V    Lux | ADC  V    Lux | ADC  V    Lux |\n"
            header += "----+---------------+---------------+---------------+---------------+\n"
            
            table = ""
            for row in range(5):
                table += f" {row+1}  |"
                for col in range(4):
                    lux = self.light_values[row][col]
                    adc = int(min(4095, max(0, lux * 2)))  # Simulate ADC value
                    voltage = adc * 3.3 / 4095  # Simulate voltage
                    table += f" {adc:4d} {voltage:.2f}V {lux:.1f} |"
                table += "\n"
            
            footer = "===========================================================\n"
            result = f"\nISO {self.iso}, {shutter} (EV: {ev:.1f})\n"
            mode = f"Metering mode: {self.metering_type}\n"
            
            self.log(header + table + footer + result + mode)
            
            # Update the display
            self.update_display()
            self.update_light_matrix()

    def update_display(self):
        # Clear canvas
        self.display_canvas.delete("all")
        
        # Draw based on current screen
        if self.current_screen == "main":
            self.draw_main_screen()
        elif self.current_screen == "menu":
            self.draw_menu_screen()
        elif self.current_screen == "config_iso":
            self.draw_config_screen("ISO", f"{self.iso}")
        elif self.current_screen == "config_type":
            self.draw_config_screen("Metering Type", self.metering_type.capitalize())
        elif self.current_screen == "config_calibration":
            self.draw_config_screen("Calibration", f"{self.calibration}")

    def draw_main_screen(self):
        # Title
        self.display_canvas.create_text(400, 30, text="4x5 LIGHTMETER", font=("Arial", 16, "bold"), fill="black")
        
        # Settings summary
        self.display_canvas.create_text(400, 60, text=f"ISO {self.iso} | {self.metering_type.capitalize()} | Cal {self.calibration}", 
                                         font=("Arial", 10), fill="black")
        
        # Shutter speed (large center display)
        self.display_canvas.create_text(400, 120, text=self.shutter_speed, 
                                        font=("Arial", 36, "bold"), fill="black")
        
        # EV value
        self.display_canvas.create_text(400, 170, text=f"EV: {self.ev_value}", 
                                       font=("Arial", 14), fill="black")
        
        # Help text
        self.display_canvas.create_text(400, 220, text="Long press MEASURE for menu", 
                                       font=("Arial", 8), fill="gray")

    def draw_menu_screen(self):
        # Title
        self.display_canvas.create_text(400, 30, text="MENU", font=("Arial", 16, "bold"), fill="black")
        
        # Draw menu options
        y_start = 80
        for i, option in enumerate(self.menu_options):
            # Highlight selected option
            if i == self.menu_index:
                # Selected option
                self.display_canvas.create_rectangle(250, y_start-15, 550, y_start+15, 
                                                     fill="black", outline="black")
                self.display_canvas.create_text(400, y_start, text=option, 
                                               font=("Arial", 14, "bold"), fill="white")
            else:
                # Normal option
                self.display_canvas.create_text(400, y_start, text=option, 
                                               font=("Arial", 14), fill="black")
            y_start += 40
        
        # Help text
        self.display_canvas.create_text(400, 220, text="UP/DOWN to navigate, MEASURE to select", 
                                       font=("Arial", 8), fill="gray")

    def draw_config_screen(self, setting_name, current_value):
        # Title
        self.display_canvas.create_text(400, 30, text=f"Config: {setting_name}", 
                                        font=("Arial", 16, "bold"), fill="black")
        
        # Current value (large center display)
        self.display_canvas.create_text(400, 120, text=current_value, 
                                        font=("Arial", 24, "bold"), fill="black")
        
        # Help text
        self.display_canvas.create_text(400, 180, text="UP/DOWN to change value", 
                                       font=("Arial", 10), fill="gray")
        self.display_canvas.create_text(400, 210, text="MEASURE to save and return", 
                                       font=("Arial", 10), fill="gray")

    def update_light_matrix(self):
        # Update debug light matrix visualization
        if not hasattr(self, 'light_matrix') or not self.light_matrix:
            return
            
        max_lux = max([max(row) for row in self.light_values])
        
        for row in range(5):
            for col in range(4):
                lux = self.light_values[row][col]
                # Calculate grayscale color (brighter = more light)
                intensity = int(220 - min(220, (lux / max_lux) * 220))
                color = f"#{intensity:02x}{intensity:02x}{intensity:02x}"
                
                # Highlight spots used by current metering mode
                if (self.metering_type == "spot" and row == 2 and (col == 1 or col == 2)):
                    # Highlight spot meter LEDs (10 and 11)
                    border_color = "red"
                    border_width = 2
                else:
                    border_color = "black"
                    border_width = 1
                
                self.debug_matrix_canvas.itemconfig(self.light_matrix[row][col], 
                                                   fill=color, 
                                                   outline=border_color,
                                                   width=border_width)

    def log(self, text):
        self.console.insert(tk.END, text + "\n")
        self.console.see(tk.END)
        
        # Parse any measurement data
        self.parse_response(text)

    def parse_response(self, text):
        # Extract shutter speed and EV
        shutter_match = re.search(r'ISO (\d+), ([\d/\.]+)\s*(?:seconds)?\s*\(EV: ([\d\.]+)\)', text)
        if shutter_match:
            iso = int(shutter_match.group(1))
            shutter = shutter_match.group(2)
            ev = shutter_match.group(3)
            
            self.iso = iso
            self.shutter_speed = shutter
            self.ev_value = ev
            self.update_display()
        
        # Extract metering mode
        mode_match = re.search(r'Metering mode: (\w+)', text)
        if mode_match:
            self.metering_type = mode_match.group(1).lower()
            self.update_display()

    def toggle_demo_mode(self):
        self.demo_mode = not self.demo_mode
        if self.demo_mode:
            self.log("Demo mode enabled - device simulation active")
            if self.serial:
                self.toggle_connection()  # Disconnect if connected
        else:
            self.log("Demo mode disabled - connect to a real device")

    def randomize_values(self):
        if not self.demo_mode:
            return
            
        # Randomize light values for demo
        for row in range(5):
            for col in range(4):
                self.light_values[row][col] = random.uniform(10, 500)
        
        self.simulate_measurement()

# Start the application
if __name__ == "__main__":
    root = tk.Tk()
    app = LightMeterUI(root)
    root.mainloop()