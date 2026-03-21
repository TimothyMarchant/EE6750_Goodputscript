# !/bin/bash

NumberOfCarsArray=(2 4 6)
ScriptName="real_per_goodput_ring.cc"
StartSeed=1
EndSeed=3

#./ns3 run scratch/$ScriptName -- --Filename="${NumberOfCarsArray[0]}_Cars_Goodput" --Foldername="${NumberOfCarsArray[0]}_Cars_Output" --seed=1 --numVehicles=${NumberOfCarsArray[0]}

for i in ${NumberOfCarsArray[@]};
do
    for ((j = $StartSeed ; j <= $EndSeed ; j++));
    do
        ./ns3 run scratch/$ScriptName -- --Filename=$i"_Cars_Goodput" --Foldername=$i"_Cars_Output" --seed=$j --numVehicles=$i
    done
done
