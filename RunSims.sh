# !/bin/bash

NumberOfCarsArray=(2 4 6)
ScriptName="real_per_goodput_ring.cc"


./ns3 run scratch/$ScriptName -- --Filename="${NumberOfCarsArray[0]}_Cars_Goodput" --Foldername="${NumberOfCarsArray[0]}_Cars_Output" --seed=1 --numVehicles=${NumberOfCarsArray[0]}
