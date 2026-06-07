import serial
import re
import sys
import matplotlib.pyplot as plt
import time

# Connect the board over UART and make sure nothing else is using that com port
COM_PORT = input("enter com port OR file name: ")
BAUD_RATE = 115200    


# SOAR_PRINT("TC@%u: %f, %f, %f (%lu)\n", ...)
tc_pattern = re.compile(r"TC@\d+:\s+([-\d\.]+),\s+([-\d\.]+),\s+([-\d\.]+)\s+\((\d+)\)")

# SOAR_PRINT("PT@%u: %f, %f, (%lu)\n", ...)
pt_pattern = re.compile(r"PT@\d+:\s+([-\d\.]+),\s+([-\d\.]+),\s+\((\d+)\)")

fcbpt_pattern = re.compile(r"PT@\d+:\s+([-\d\.]+),\s+\((\d+)\)")

tc_timestamps, tc_temp1, tc_temp2, tc_temp3 = [], [], [], []
pt_timestamps, pt_press1, pt_press2 = [], [], []
fcbpt_timestamps, fcbpt_press1 = [], []

def process(line):
    tc_match = tc_pattern.search(line)
    if tc_match:
        tc_temp1.append(float(tc_match.group(1)))
        tc_temp2.append(float(tc_match.group(2)))
        tc_temp3.append(float(tc_match.group(3)))
        tc_timestamps.append(int(tc_match.group(4)))
        return

    pt_match = pt_pattern.search(line)
    if pt_match:
        pt_press1.append(float(pt_match.group(1)))
        pt_press2.append(float(pt_match.group(2)))
        pt_timestamps.append(int(pt_match.group(3)))

    fcbpt_match = fcbpt_pattern.search(line)
    if fcbpt_match:
        fcbpt_press1.append(float(fcbpt_match.group(1)))
        fcbpt_timestamps.append(int(fcbpt_match.group(2)))

def main():


    try:
        with serial.Serial(COM_PORT, BAUD_RATE, timeout=1) as ser:
            print(f"Listening on {COM_PORT} at {BAUD_RATE} baud...")
            print("Waiting for 'DUMPING FLASH' trigger...")
            dumping_started = False
            time.sleep(0.5)
            ser.write(b"dumpy\r\n")

            while True:
                line = ser.readline().decode('utf-8', errors='ignore').strip()

                if not line:
                    continue

                print(line) 

                if "DUMPING FLASH" in line:
                    dumping_started = True
                    print("STARTING... Collecting data...")
                    continue

                if "DONE!!!!!!!!!!!!" in line:
                    print("IT IS DONE")
                    break

                if dumping_started:
                    process(line)
                else:
                    ser.write(b"dumpy\r\n")


    except serial.SerialException as e:
        try:
             with open(COM_PORT, "r") as f:
                  for i in f.readlines():
                       process(i.strip())
             
        except:
            print(f"Error opening serial port: {e}")
            print("You probably have termite open...... or got the com number wrong..... make sure to enter \"COM[X]\"")
            sys.exit(1)
            
    except KeyboardInterrupt:
        print("\nInterrupted. graphing")

    if not tc_timestamps and not pt_timestamps:
        print("no data. check your stuff")
        sys.exit(0)

    # Convert timestamps from ms (FreeRTOS ticks) to seconds for the X-axis
    tc_time_sec = [t / 1000.0 for t in tc_timestamps]
    pt_time_sec = [t / 1000.0 for t in pt_timestamps]
    fcbpt_time_sec = [t / 1000.0 for t in fcbpt_timestamps]

    tcs = []
    pts = []
    fcbpts = []
    for i in range(0,len(tc_time_sec)):
         tcs.append((tc_temp1[i],tc_temp2[i],tc_temp3[i],tc_time_sec[i]))
    for i in range(0,len(pt_time_sec)):
         pts.append((pt_press1[i],pt_press2[i],pt_time_sec[i]))
    for i in range(0,len(fcbpt_time_sec)):
         fcbpts.append((fcbpt_press1[i],fcbpt_time_sec[i]))

    tcs.sort(key = lambda x: x[-1])
    pts.sort(key = lambda x: x[-1])
    fcbpts.sort(key = lambda x: x[-1])
    
    # backup, just save to file
    with open(f"rawdumpdata{'FCB' if len(fcbpts) else 'PBB'}.txt",mode="w") as f:
        for i in range(0,len(tcs)):
            f.write(f"TC@{i}: {tcs[i][0]}, {tcs[i][1]}, {tcs[i][2]} ({int(tcs[i][3]*1000)})\n")
        for i in range(0,len(pt_time_sec)):
            f.write(f"PT@{i}: {pts[i][0]}, {pts[i][1]}, ({int(pts[i][2]*1000)})\n")
        for i in range(0,len(fcbpt_time_sec)):
            f.write(f"PT@{i}: {fcbpts[i][0]}, ({int(fcbpts[i][1]*1000)})\n")

    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(12, 8))

    if tc_time_sec:
        ax1.plot([m[-1] for m in tcs], [m[0] for m in tcs], label='TC 1', color='red', alpha=0.8)
        ax1.plot([m[-1] for m in tcs], [m[1] for m in tcs], label='TC 2', color='orange', alpha=0.8)
        ax1.plot([m[-1] for m in tcs], [m[2] for m in tcs], label='TC 3', color='blue', alpha=0.8)
        ax1.set_title('Thermocouple Data')
        ax1.set_ylabel('Temperature (°C)')
        ax1.grid(True, linestyle='--', alpha=0.6)
        ax1.legend()
    else:
        ax1.text(0.5, 0.5, 'No TC Data Collected', ha='center', va='center')

    if pt_time_sec:
        ax2.plot([m[-1] for m in pts], [m[0] for m in pts], label='Pressure 1', color='purple', alpha=0.8)
        ax2.plot([m[-1] for m in pts], [m[1] for m in pts], label='Pressure 2', color='green', alpha=0.8)
        ax2.set_title('PBB Pressure Transducer Data')
        ax2.set_xlabel('Time (Seconds)')
        ax2.set_ylabel('Pressure')
        ax2.grid(True, linestyle='--', alpha=0.6)
        ax2.legend()
    else:
        ax2.text(0.5, 0.5, 'No PBB PT Data Collected', ha='center', va='center')

    if fcbpt_time_sec:
        ax3.plot([m[-1] for m in fcbpts], [m[0] for m in fcbpts], label='FCB Pressure 1', color='purple', alpha=0.8, marker="o")
        ax3.set_title('FCB Pressure Transducer Data')
        ax3.set_xlabel('Time (Seconds)')
        ax3.set_ylabel('Pressure')
        ax3.grid(True, linestyle='--', alpha=0.6)
        ax3.legend()
    else:
        ax3.text(0.5, 0.5, 'No FCB PT Data Collected', ha='center', va='center')

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    main()
