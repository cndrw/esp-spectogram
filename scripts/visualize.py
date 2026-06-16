
import matplotlib.pyplot as plt
import pandas as pd

data_raw = pd.read_csv("out.csv", delimiter=" ")
data = data_raw["data"]
print(len(data))
plt.plot(data)
plt.show()
exit(0)