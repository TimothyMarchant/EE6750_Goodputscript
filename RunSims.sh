# !/bin/bash
ProgressBar(){
    Total=$1
    Current=$2

    printf $2"/"$1"\n"
}
NumberOfCarsArray=(8 10 12)
SeedsForFourCars=(1 3 4 5 6)
ScriptName="QUICVsTCPSimulation.cc"
StartSeed=1
EndSeed=5
perlist=(0 0.1)
declare -i TotalRuns=($EndSeed*${#NumberOfCarsArray[@]})
CurrentRun=(0)
RunsPerVehicleCount=5
#Run loop through the amount of cars in the car number array

    #run over several trials.  You should get similiar results.
    for ((j = 0 ; j < $EndSeed ; j++));
    do 
       
        ProgressBar $RunsPerVehicleCount $j
        ./ns3 run scratch/$ScriptName -- --Filename="4_Cars_Data" --Foldername="4_Cars_Output" --seed=${SeedsForFourCars[$j]} --numVehicles=4 --verbose=1
        sleep 1
    done
echo "DONE"
