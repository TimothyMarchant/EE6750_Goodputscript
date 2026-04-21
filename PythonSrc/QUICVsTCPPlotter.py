import pyshark
#import numpy as np
print("FIRST")
CAP=pyshark.FileCapture('PCAP/1_goodput_QUIC-0-1.pcap')
print("RAN")
print(CAP[0])