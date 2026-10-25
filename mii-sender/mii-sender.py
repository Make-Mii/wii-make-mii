import socket, os

#WII_IP = "192.168.137.39"  # Replace with the IP shown on the top of your Wii screen
WII_IP = "10.249.68.99"  # Replace with the IP shown on the top of your Wii screen
PORT = 8080
mi_path = "mi.mii"

# Load an existing .mii file, or generate 74 random bytes to test
if os.path.exists(mi_path):
    with open(mi_path, "rb") as f:
        mii_data = f.read(74)
else:
    mii_data = os.urandom(74)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.sendto(mii_data, (WII_IP, PORT))
print("Sent 74-byte .mii packet!")