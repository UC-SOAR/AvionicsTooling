import serial
import serial.tools.list_ports
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import defaultdict
import time
import atexit

# config
BAUD_RATE = 115200
MAX_POINTS = 100

def select_port():
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("No serial ports found.")
        exit(1)

    # list available ports
    print("\nAvailable serial ports:")
    for i, p in enumerate(ports):
        print(f"  [{i}] {p.device} - {p.description}")

    print(f"\nEnter port number or name (e.g. COM3 or /dev/ttyUSB0): ", end="")
    user_input = input().strip()

    # input selected com port by index or name
    if user_input.isdigit():
        idx = int(user_input)
        if 0 <= idx < len(ports):
            return ports[idx].device
        else:
            print("Invalid index.")
            exit(1)
    else:
        return user_input

# select serial port
SERIAL_PORT = select_port()

tasks_data = defaultdict(list)

# serial setup
ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1, dsrdtr=False, rtscts=False)
time.sleep(3)

# start profiling
ser.write(b'\r\n')
time.sleep(1)
ser.write(b'profile\r\n')
print(f"Connected to {SERIAL_PORT}. Profiling started...")

# parse profile messages
def parse_line(line):
    if '\x1b' in line:
        return None

    parts = [p.strip() for p in line.split(',')]
    if len(parts) < 5:
        return None

    task = parts[0].strip()

    if task == 'Task Name' or not task:
        return None

    cpu_str = parts[4].strip()
    cpu_str = cpu_str.replace('<', '').replace('%', '').strip()
    try:
        cpu = float(cpu_str)
    except ValueError:
        return None

    return task, cpu

# plot cpu usage
fig, ax = plt.subplots(figsize=(10, 6))

def animate(frame):
    while ser.in_waiting:
        raw = ser.readline().decode(errors='ignore').strip()
        if raw:
            parsed = parse_line(raw)
            if parsed:
                task, cpu = parsed
                tasks_data[task].append(cpu)
                if len(tasks_data[task]) > MAX_POINTS:
                    tasks_data[task].pop(0)

    ax.clear()
    ax.set_title(f"Task CPU Usage — {SERIAL_PORT}")
    ax.set_xlabel("Samples")
    ax.set_ylabel("CPU Usage (%)")
    ax.set_ylim(0, 105)
    ax.grid(True, alpha=0.3)

    for task, data in tasks_data.items():
        ax.plot(data, label=f"{task} (latest: {data[-1]:.0f}%)" if data else task, marker='o', markersize=3)

    ax.legend(loc='upper left')
    fig.tight_layout()

# stop profiling on exit
def stop_profiling():
    try:
        if ser.is_open:
            ser.write(b'stop profiling\r\n')
            time.sleep(1)
            print("\nProfiling stopped.")
            ser.close()
    except Exception as e:
        print(f"Cleanup error: {e}")

ani = animation.FuncAnimation(fig, animate, interval=2000, cache_frame_data=False)

try:
    plt.show()
except KeyboardInterrupt:
    pass
finally:
    stop_profiling()