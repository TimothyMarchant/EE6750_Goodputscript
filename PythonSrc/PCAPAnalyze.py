import pyshark
import numpy as np
import os

Experiment1Folder="VehicleExperiment_OutputPCAPS"
Experiment2Folder="SpeedExperiment_OutputPCAPS"
Experiment3Folder="PERExperiment_OutputPCAPS"
ExperimentFolders=[Experiment1Folder,Experiment2Folder,Experiment3Folder]
def ReadPCAPs(Path,OutputPath):
    PCAPFolder="PCAP/"
    PCAPFiles=os.listdir(Path+"/"+PCAPFolder)
    arr=np.zeros(shape=(len(PCAPFiles)))
    for i in range (0,len(PCAPFiles)):
        CAP=pyshark.FileCapture(Path+"/"+PCAPFolder+PCAPFiles[i],display_filter='wlan.fc.retry == True')
        index=0
        for j in CAP:
            index=index+1
        #temp=np.array(CAP)
        arr[i]=index
        print(i)
    np.savetxt(OutputPath+".csv",arr)

for i in ExperimentFolders:
    ReadPCAPs(i+"/"+"QUIC",i+"_QUIC_MACRETRIES")
    print("Finished half of: "+i)
    ReadPCAPs(i+"/"+"TCP",i+"_TCP_MACRETRIES")
    print("Finished: "+i)
print("Finished getting mac retries")