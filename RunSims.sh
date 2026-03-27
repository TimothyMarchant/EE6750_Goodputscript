# !/bin/bash
ProgressBar(){
    Total=$1
    Current=$2

    printf $2"/"$1"\n"
}
NumberOfCarsArray=(8 10 12)
SeedsForFourCars=(1 2 3 4 5 6 7 8 9 10)
ScriptName="QUICVsTCPSimulation.cc"
StartSeed=1
EndSeed=1000
perlist=(0 0.1)
declare -i TotalRuns=($EndSeed*${#NumberOfCarsArray[@]})
CurrentRun=(0)
RunsPerVehicleCount=1000
DebugList=QuicBbr:QuicClient:QuicCongestionControl:QuicEchoHelper:QuicEchoServerApplication:QuicHeader:QuicHelper:QuicL4Protocol:QuicL5Protocol:QuicServer:QuicSocket:QuicSocketBase:QuicSocketFactory:QuicSocketRxBuffer:QuickSocketTxBuffer:QuicSocketTxEdfScheduler:QuicSocketTxPFifoScheduler:QuicSocketTxScheduler:QuicStream:QuicStreamBase:QuicStreamRxBuffer:QuicStreamTxBuffer:QuicSubheader:QuicTransportParameters
    #4 cars
    echo "4 cars"
    #run over several trials.  You should get similiar results.
    for ((j = 1 ; j < $EndSeed ; j++));
    do 
       
        ProgressBar $RunsPerVehicleCount $j
        ./ns3 run scratch/$ScriptName -- --Filename="4_Cars_Data" --Foldername="4_Cars_Output" --seed=$j --numVehicles=4 --verbose=0 --packetSize=512 --Streams=1
        #sleep 1
    done

    #8 cars
    #run over several trials.  You should get similiar results.
echo "8 cars"
    for ((j = 1 ; j < $EndSeed ; j++));
    do 
        #NS_LOG="*=level_error" 
        ProgressBar $RunsPerVehicleCount $j
        ./ns3 run scratch/$ScriptName -- --Filename="8_Cars_Data" --Foldername="8_Cars_Output" --seed=$j --numVehicles=8 --verbose=0 --overWrite=1 --TCPOrQUIC=0 --packetSize=512 --Streams=1

	
    done
   echo "12 cars" 
        #12 cars
    #run over several trials.  You should get similiar results.

    for ((j = 1 ; j < $EndSeed ; j++));
    do 
        #NS_LOG="*=level_error" 
        ProgressBar $RunsPerVehicleCount $j
        ./ns3 run scratch/$ScriptName -- --Filename="12_Cars_Data" --Foldername="12_Cars_Output" --seed=$j --numVehicles=12 --verbose=0 --overWrite=1 --TCPOrQUIC=0 --packetSize=512 --Streams=1


    done
echo "20 cars"
#20 cars
    for ((j = 1 ; j < $EndSeed ; j++));
    do 
        #NS_LOG="*=level_error" 
        ProgressBar $RunsPerVehicleCount $j
        ./ns3 run scratch/$ScriptName -- --Filename="20_Cars_Data" --Foldername="20_Cars_Output" --seed=$j --numVehicles=20 --verbose=0 --overWrite=1 --TCPOrQUIC=0 --packetSize=512 --Streams=1

	
    done
echo "DONE"
