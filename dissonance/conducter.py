# conductor.py
import socket, time

HOST = "192.168.4.1"  # default SoftAP IP of ESP32 host (change if your AP differs)
PORT = 5000
SCHEDULE_AHEAD = 0.45  # seconds – how far into future to schedule (tweak: 0.2-0.6)


def send_line(sock, line):
    sock.sendall((line + "\n").encode("utf-8"))
    print("Sent:", line)


def schedule_sequence(sock):
    # sequence for example: C3 is host but C3 will also receive PLAYs; we send to all
    notes = ["C3", "D3", "E3", "F3", "G3", "A3", "B3", "C4"]
    dur = 0.25  # quarter-note spacing in seconds (example)
    now = time.time()
    for i, note in enumerate(notes):
        target = now + SCHEDULE_AHEAD + i * dur
        line = f"PLAY {note} {target:.6f}"
        send_line(sock, line)
        # optionally small sleep between sends
        time.sleep(0.005)


def schedule_warmpups(sock):
    notes = ["C4", "G3", "A3", "F3", "G3", "E3", "F3", "D3", "E3", "C3"] * 4
    dur = 0.25 / 4
    now = time.time()
    for i, note in enumerate(notes):
        target = now + SCHEDULE_AHEAD + i * dur
        line = f"PLAY {note} {target:.6f}"
        send_line(sock, line)
        # optionally small sleep between sends
        time.sleep(0.001)


def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((HOST, PORT))
    print("Connected to host.")
    # Optionally: inform host of absolute time so it can set its unix clock base:
    send_line(sock, f"SET_TIME {time.time():.6f}")
    time.sleep(0.05)
    # Send one sequence
    while True:
        schedule_sequence(sock)
        time.sleep(0.25 * 12)
        schedule_warmpups(sock)
        time.sleep(0.05 * 44)
    sock.close()


if __name__ == "__main__":
    main()
