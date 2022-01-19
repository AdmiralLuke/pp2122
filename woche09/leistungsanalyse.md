## Aufgabenblatt 9
---

| K   | P   | N     | I   | Berechnungszeit | +-     | Berechnungszeit 1 Kern 1 Task | Berechnungszeit Jacobi |
| --- | --- | ----- | --- | --------------- | ------ | ----------------------------- | ---------------------- |
| 1   | 1   | 836   | 1024 | 256.980s       | 0.418s | 258.794s                      | 248.598s
| 1   | 2   | 1182  | 1024 | 184.614s       | 0.412s | 365.092s                      | 177.514s
| 1   | 3   | 1448  | 1024 | 151.884s       | 0.432s | 447.884s                      | 144.160s
| 1   | 6   | 2048  | 1024 | 108.896s       | 0.802s | 631.420s                      | 104.810s
| 1   | 12  | 2896  | 1024 | 82.392s        | 0.264s | 891.842s                      | 78.960s
| 1   | 24  | 4096  | 1024 | 64.812s        | 0.936s | 1269.04s                      | 62.206s
| 2   | 48  | 5793  | 1024 | 48.576s        | 0.429s | 1784.43s                      | 47.142s
| 4   | 96  | 8192  | 1024 | 37.688s        | 0.524s | -                             | 35.314s
| 8   | 192 | 11585 | 1024 | 33.092s        | 2.675s | -                             | 28.498s


![[Pasted image 20220119180826.png]]


##### Hardware: ant13 (hauptsächlich, außerdem ant14-20)
>NodeName=ant13 Arch=x86_64 CoresPerSocket=24
   CPUAlloc=6 CPUTot=48 CPULoad=2.72
   AvailableFeatures=(null)
   ActiveFeatures=(null)
   Gres=(null)
   NodeAddr=ant13 NodeHostName=ant13 Version=20.11.8
   OS=Linux 4.18.0-348.2.1.el8_5.x86_64 #1 SMP Tue Nov 16 14:42:35 UTC 2021
   RealMemory=120536 AllocMem=12288 FreeMem=109075 Sockets=1 Boards=1
   State=MIXED ThreadsPerCore=2 TmpDisk=0 Weight=1 Owner=N/A MCS_label=N/A
   Partitions=vl-parcio
   BootTime=2021-11-22T12:56:23 SlurmdStartTime=2021-11-22T12:57:10
   CfgTRES=cpu=48,mem=120536M,billing=48
   AllocTRES=cpu=6,mem=12G
   CapWatts=n/a
   CurrentWatts=0 AveWatts=0
   ExtSensorsJoules=n/s ExtSensorsWatts=0 ExtSensorsTemp=n/s
   Comment=(null)
   
   ##### Interpretation
   Die Werte zeigen eine geeignete Parallelisierung. Zusätzlich sieht man, das die Gauß-Seidel implementation etwas langsamer ist, als die von Jacobi. 


