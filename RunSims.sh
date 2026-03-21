# !/bin/bash
ProgressBar(){
    Total=$1
    Current=$2

    printf $2"/"$1"\r"
}
NumberOfCarsArray=(2 4 6 8 10 12 14 16 18 20)

ScriptName="real_per_goodput_ring.cc"
StartSeed=1
EndSeed=2
declare -i TotalRuns=($EndSeed*${#NumberOfCarsArray[@]})
CurrentRun=0
#Run loop through the amount of cars in the car number array
for i in ${NumberOfCarsArray[@]};
do

    #run over several trials.  You should get similiar results.
    for ((j = $StartSeed ; j <= $EndSeed ; j++));
    do 
        declare -i Current=($CurrentRun*$EndSeed+$j)
        ProgressBar $TotalRuns $Current
        ./ns3 run scratch/$ScriptName -- --Filename=$i"_Cars_Goodput" --Foldername=$i"_Cars_Output" --seed=$j --numVehicles=$i --verbose=0
    done
    CurrentRun=($CurrentRun+1)
done
echo "DONE"