# Multistage Robust Cutting Stock Problem

This repository contains code used for computational experiments on multistage robust cutting-stock instances.  The code can generate benchmark instances, write several deterministic model files, and run a decision-rule algorithm that uses CPLEX.
Instance generation is separated from model generation. Instances are stored as `.raw` files and can then be reused by the different model writers and algorithms.

## Build
The non-CPLEX tools can be built with CMake:
```bash
cmake -S . -B build
cmake --build build
```
This builds:
```text
build/GenerateInstances
build/GenerateModelFiles
```
The decision-rule executable requires IBM ILOG CPLEX.  Build it by enabling `BUILD_DECISION_RULE` and pointing CMake to your CPLEX installation, for example on Linux:
```bash
cmake -S . -B build \
  -DBUILD_DECISION_RULE=ON \
  -DCPLEX_ROOT=/opt/ibm/ILOG/CPLEX_Studio2211 \
  -DCPLEX_ARCH=x86-64_linux
cmake --build build
```
On macOS the architecture directory may be `arm64_osx` or `x86-64_osx`, depending on the CPLEX installation.

## Generate raw instances
`GenerateInstances` creates `.raw` instance files only.  It does not create LP, MPS, or QLP model files.
Create one instance:
```bash
./build/GenerateInstances T B I seed output_directory
```
Create several instances with consecutive seeds:
```bash
./build/GenerateInstances T B I first_seed last_seed output_directory
```
Example:
```bash
./build/GenerateInstances 9 4 5 190 199 instances/data
```
This creates the ten files
```text
binPacking_9_4_5_190.raw
...
binPacking_9_4_5_199.raw
```
in `instances/data`.  To reduce the runtime of an experiment, use a smaller seed range.

## Generate selected model files
`GenerateModelFiles` reads an existing `.raw` file and writes deterministic model files.  By default, all supported model files are written:
```bash
./build/GenerateModelFiles instance.raw
```
An optional output base can be supplied:
```bash
./build/GenerateModelFiles instance.raw output/base/name
```
Large model files can be skipped with `--no-*` flags.  For example, to generate only the QIP and RC files:
```bash
./build/GenerateModelFiles instance.raw --no-ro --no-ro-rc --no-mip
```
Available skip flags are:
```text
--no-qip
--no-ro
--no-ro-rc
--no-mip
--no-rc
```

## Run the decision-rule algorithm
`RunDecisionRule` reads a `.raw` instance and runs the decision-rule approach as presented in [1].
```bash
./build/RunDecisionRule instance.raw P h [timelimit_seconds] [output_base]
```
Here `P` is the number of pieces in the binary decision rule and `h` is the number of previous stages included in the information window.  The default global time limit is 3600 seconds; pass `0` to disable it.
Example:
```bash
./build/RunDecisionRule instances/data/binPacking_9_4_5_190.raw 1 3 1800
```
A successful run prints summary lines such as:
```text
TIME: ...
OBJ: ...
SCENARIOS: ...
ITERATIONS: ...
STATUS: FINISHED
```
Possible statuses include:
```text
FINISHED      the decision-rule algorithm terminated successfully
TIMELIMIT     the global wall-clock time limit was reached
INFEASIBLE    no decision rule of the requested type exists
ERROR         the solver stopped without a usable solution for another reason
```

## References
[1] Bertsimas, Dimitris, and Angelos Georghiou. "Design of near optimal decision rules in multistage adaptive mixed-integer optimization." Operations Research 63.3 (2015): 610-627.
