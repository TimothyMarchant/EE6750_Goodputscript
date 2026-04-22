import pyshark
import numpy as np
import os

Experiment1Folder="VehicleExperiment_OutputPCAPS"
Experiment2Folder="SpeedExperiment_OutputPCAPS"
Experiment3Folder="PERExperiment_OutputPCAPS"
ExperimentFolders=[Experiment1Folder,Experiment2Folder,Experiment3Folder]
def ReadPCAPs(Path,OutputPath):

    PCAPFiles=os.listdir(Path)
    arr=np.zeros(shape=(len(PCAPFiles)))
    for i in range (0,len(PCAPFiles)):
        CAP=pyshark.FileCapture(Path+PCAPFiles[i],display_filter='wlan.fc.retry == True')
        index=0
        for j in CAP:
            index=index+1
        #temp=np.array(CAP)
        arr[i]=index
        print(i)
    np.savetxt(OutputPath+".csv",arr)
QUICOutputFolder="QUICMACRETRYS"
TCPOutputFolder="TCPMACRETRYS"

try:
    os.mkdir(QUICOutputFolder)
    os.mkdir(TCPOutputFolder)

except Exception as e:
    print(e)
for i in ExperimentFolders:
    QUICSubFolders=os.listdir(i+"/QUIC/PCAP")
    TCPSubFolders=os.listdir(i+"/TCP/PCAP")
    for j in QUICSubFolders:
        ReadPCAPs(i+"/"+"QUIC/PCAP/"+j+"/",QUICOutputFolder+"/"+i+"_"+j+"_QUIC_MACRETRIES")
    print("Finished half of: "+i)
    for j in TCPSubFolders:
        ReadPCAPs(i+"/"+"TCP/PCAP/"+j+"/",TCPOutputFolder+"/"+i+"_"+j+"_TCP_MACRETRIES")
    print("Finished: "+i)
print("Finished getting mac retries")