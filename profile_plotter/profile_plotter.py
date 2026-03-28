import serial
import serial.tools.list_ports
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import defaultdict
import time

# config
BAUD_RATE = 115200
MAX_POINTS = 100
WORD_SIZE = 4

def select_port():
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("No serial ports found.")
        exit(1)

    print("\nAvailable serial ports:")
    for i, p in enumerate(ports):
        print(f"  [{i}] {p.device} - {p.description}")

    user_input = input("\nEnter port number or name: ").strip()

    if user_input.isdigit():
        idx = int(user_input)
        if 0 <= idx < len(ports):
            return ports[idx].device
        else:
            print("Invalid index.")
            exit(1)
    else:
        return user_input

SERIAL_PORT = select_port()

# empty dicts for cpu, stack high water
tasks_data = defaultdict(list)
stack_data = defaultdict(list)
latest_free_heap = None

# serial setup
ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1, dsrdtr=False, rtscts=False)
time.sleep(3)

ser.write(b'\r\n')
time.sleep(1)
ser.write(b'profile\r\n')
print(f"Connected to {SERIAL_PORT}. Profiling started...")

def parse_line(line):
    if '\x1b' in line:
        return None

    # parse free heap
    if "Free Heap" in line:
        return ("heap_header", None)

    parts = [p.strip() for p in line.split(',')]

    # actual heap values
    if len(parts) == 2:
        try:
            free_heap = int(parts[0])
            return ("heap", free_heap)
        except ValueError:
            return None

    # parse task entry
    if len(parts) < 5:
        return None

    task = parts[0]
    if task == 'Task Name' or not task:
        return None

    # convert stack high water mark to bytes
    try:
        stack_words = int(parts[3])
        stack_bytes = stack_words * WORD_SIZE
    except ValueError:
        return None

    # parse CPU usage
    cpu_str = parts[4].replace('<', '').replace('%', '').strip()
    try:
        cpu = float(cpu_str)
        if cpu < 1: # for display reasons round values <1 to 1% CPU use
            cpu = 1.0
    except ValueError:
        return None

    return ("task", task, cpu, stack_bytes)

# init figure with 2 plots (CPU, stack use)
fig, (ax_cpu, ax_mem) = plt.subplots(2, 1, figsize=(10, 10))

def animate(frame):
    global latest_free_heap
    while ser.in_waiting:
        raw = ser.readline().decode(errors='ignore').strip()
        if raw:
            parsed = parse_line(raw)

            if not parsed:
                continue

            # CPU data
            if parsed[0] == "task":
                _, task, cpu, stack_bytes = parsed

                tasks_data[task].append(cpu)
                stack_data[task].append(stack_bytes)

                if len(tasks_data[task]) > MAX_POINTS:
                    tasks_data[task].pop(0)
                if len(stack_data[task]) > MAX_POINTS:
                    stack_data[task].pop(0)

            # heap data
            elif parsed[0] == "heap":
                _, free_heap = parsed
                latest_free_heap = free_heap

    # create CPU plot
    ax_cpu.clear()
    ax_cpu.set_title(f"Task CPU Usage — {SERIAL_PORT}")
    ax_cpu.set_ylabel("CPU (%)")
    ax_cpu.set_ylim(0, 105)
    ax_cpu.grid(True, alpha=0.3)

    for task, data in tasks_data.items():
        if data:
            ax_cpu.plot(data, label=f"{task} ({data[-1]:.0f}%)", marker='o', markersize=3)

    ax_cpu.legend(loc='upper left')

    # create stack usage plot
    ax_mem.clear()
    ax_mem.set_title("Memory Usage")
    ax_mem.set_xlabel("Samples")
    ax_mem.set_ylabel("Bytes")
    ax_mem.grid(True, alpha=0.3)

    # stack use for each task
    for task, data in stack_data.items():
        if data:
            ax_mem.plot(data, linestyle='--', label=f"{task} stack ({data[-1]} B)")

    # add free heap to legend
    if latest_free_heap is not None:
        ax_mem.plot([], [], ' ', label=f"Free Heap ({latest_free_heap} B)")

    ax_mem.legend(loc='upper left')
    fig.tight_layout()

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