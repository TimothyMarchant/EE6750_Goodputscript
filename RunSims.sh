# !/bin/bash
ProgressBar(){
    Total=$1
    Current=$2

    printf $2"/"$1"\n"
}

ScriptName="QUICVsTCPSimulation.cc"
StartSeed=1
EndSeed=50
PerList="0,0.1,0.2,0.4,0.6,0.8"
declare -i TotalRuns=($EndSeed*${#NumberOfCarsArray[@]})
VehicleList="2,4,6,8,10,12"
DefaultCarCount="6"
DefaultSpeed="15"
SpeedList="10,15,20,25,30,35,40"
CurrentRun=(0)
RunsPerVehicleCount=$EndSeed


DebugList=QuicBbr:QuicClient:QuicCongestionControl:QuicEchoHelper:QuicEchoServerApplication:QuicHeader:QuicHelper:QuicL4Protocol:QuicL5Protocol:QuicServer:QuicSocket:QuicSocketBase:QuicSocketFactory:QuicSocketRxBuffer:QuickSocketTxBuffer:QuicSocketTxEdfScheduler:QuicSocketTxPFifoScheduler:QuicSocketTxScheduler:QuicStream:QuicStreamBase:QuicStreamRxBuffer:QuicStreamTxBuffer:QuicSubheader:QuicTransportParameters
   
   #Experiment 1 vehicles
   echo "Experiment 1"
       #run over several trials.  You should get similiar results.
    for ((j = 1 ; j <= $EndSeed ; j++));
    do 
       
        ProgressBar $RunsPerVehicleCount $j
        ./ns3 run scratch/$ScriptName -- --Filename="VehicleExperiment_Data" --Foldername="VehicleExperiment_Output" --seed=$j --numVehicles=$VehicleList --verbose=0 --packetSize=512 --Streams=1 --perList=0 --speedList=$DefaultSpeed

        #sleep 1
    done
   
   
   #Experiment 2 speed
   echo "Experiment 2"
       #run over several trials.  You should get similiar results.
    for ((j = 1 ; j <= $EndSeed ; j++));
    do 
       
        ProgressBar $RunsPerVehicleCount $j
        ./ns3 run scratch/$ScriptName -- --Filename="SpeedExperiment_Cars_Data" --Foldername="SpeedExperiment_Cars_Output" --seed=$j --numVehicles=$DefaultCarCount --verbose=0 --packetSize=512 --Streams=1 --perList=0 --speedList=$SpeedList
        #sleep 1
    done
    #Experiment 3 PER count
    echo "Experiment 3"
    #run over several trials.  You should get similiar results.
    for ((j = 1 ; j <= $EndSeed ; j++));
    do 
       
        ProgressBar $RunsPerVehicleCount $j
        ./ns3 run scratch/$ScriptName -- --Filename="PERExperiment_Cars_Data" --Foldername="PERExperiment_Cars_Output" --seed=$j --numVehicles=$DefaultCarCount --verbose=0 --packetSize=512 --Streams=1 --speedList=$DefaultSpeed --perList=$PerList
        #sleep 1
    done


