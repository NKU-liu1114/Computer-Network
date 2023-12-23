import matplotlib.pyplot as plt


WindowSize = ['4', '12', '20', '28', '36']
ack = [0.189,0.154,0.115,0.084,0.052]

sack = [0.203,0.184,0.167,0.143,0.129]

plt.figure(figsize=(10, 5))
plt.plot(WindowSize, ack, label='ack', marker='o')
plt.plot(WindowSize, sack, label='sack', marker='s')

plt.xlabel('WindowSize')
plt.ylabel('Throughput (Mbps)')
plt.legend()

plt.grid(True)


plt.show()
