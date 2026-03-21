# !/bin/bash

NumberOfCarsArray=(2 4 6 8 10 12 14 16 18 20)
ScriptName="real_per_goodput_ring.cc"
StartSeed=1
EndSeed=10

#Run loop through the amount of cars in the car number array
for i in ${NumberOfCarsArray[@]};
do
    #run over several trials.  You should get similiar results.
    for ((j = $StartSeed ; j <= $EndSeed ; j++));
    do
        ./ns3 run scratch/$ScriptName -- --Filename=$i"_Cars_Goodput" --Foldername=$i"_Cars_Output" --seed=$j --numVehicles=$i
    done
done
