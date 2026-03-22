This repo is for our group project in EE6750 wireless communications at Kennesaw State University.  In this project we are using NS-3 with a QUIC addon to determine if QUIC(BBR) has better performance in V2V communication over WiFi than TCP(BBR).  
This repo contains scripts for running our simulations and necessary bash scripts for automating that process.  With our limited testing currently it would seem TCP has better P95 latency at the moment.  More testing needs to be done.
To run these, place RunSims.sh in the NS-3 folder, and the goodput script in NS-3/Scratch.  This requires that you have NS-3 install correctly.  

To install all the required depednecies follow these instructions.  Run these in WSL not windows git bash otherwise you will have problems (do not clone repos in windows git bash).  gcc and g++ version 9 is required.
to install version 9 of gcc and g++ run  
```bash
sudo apt install g++-9  
sudo apt install gcc-9
```
Then run
```bash
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 100
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 100
```
If you haven't done this before it should just switch to the right version.  A higher value gives the version of g++/gcc higher priority, so if it doesn't switch just use a larger value.  
To verify run
```bash
gcc --version
g++ --version
```
And finally to run this script just run
```bash
git clone https://github.com/TimothyMarchant/EE6750_NS3Installer_Bash_Script.git
cd EE6750_NS3Installer_Bash_Script
chmod +x NS3Installer.sh
./NS3Installer.sh
```
Nothing really much more than that.  However, it might take upto 15 minutes to do everything (it takes about 5-8 minutes on a desktop Intel core ultra 7 cpu and wsl using unbuntu 24.04).  
if you need to run a script we wrote just run
```bash
./ns3 run scratch/<SCRIPTNAME>.cc
```
after copying that file into the scratch folder.  Change <SCRIPTNAME> to the name of the file (e.g. if it is named QUICSimulation.cc put QUICSimulation in place of <SCRIPTNAME>).  

If you have problems running it please let me know (also you can open the script and copy the commands in it).
