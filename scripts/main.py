import serial
# import matplotlib.pyplot as plt
import csv
# import pandas as pd


# data_raw = pd.read_csv("out.csv", delimiter=" ")
# data = data_raw["data"][200:]
# plt.plot(data)
# plt.show()
# exit(0)

ser = serial.Serial('COM3', 115200, timeout=1)

x = [0]
values = [(0, 0)]

tokens = []
counter = 0

while True:
    line = ser.readline()
    if line:
        text = line.decode('utf-8', errors='ignore').strip()

        print(f"{text} ({(counter / (16_000 * 4)):.2f} %)")
        counter = counter + 1
        if (text == "end"):
            with open("out.csv", "w", encoding="utf-8") as f:
                for t in tokens:
                    try:
                        f.write(f'{int(t)}\n');
                    except:
                        pass

            exit(0)
        else:
            tokens.append(text)
        # if len(text) > 0 and text[0] == "F":
        #     ts, *v = text[2:].split(" ")
        #     values.append((ts, v[0]))
        #     if len(values) == 1_000:
        #         with open("out.csv", 'w', encoding="utf-8") as f:
        #             f.write("ts data\n")
        #             for v in values:
        #                 f.write(f"{v[0]} {v[1]}\n")

