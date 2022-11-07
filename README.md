# SO-TP2

This is an educational project for the Operating Systems class at ITBA University in Argentina.


## Compilation instructions

The executable compile.sh is meant to make compiling easier. There are a few options to compile it in different ways. The easiest way of compiling it is:
1) Run the following command once to create the necessary docker image. Replace {name} with the desired name for the docker container.
```./compile.sh docker {name}```
2) Run the following command to compile the project with the memory manager MM. Don't forget to replace MM with "standard" or "buddy". {name} must be the same as before.
```./compile.sh {MM} {name}```

## Running the project

To run the project, the software needed is the following:
1) QEMU (we personally used the version 7.1.0)
2) Docker (we used the version 20.10.12)
The command to run the project is:
```./run.sh ```
We also added an option in the run.sh executable to open the project with the GDB debugger. To use this option run the following command:
```./run.sh gdb```

## More info

Once the OS is booted, try running the command ```help```. This will display all of the possible commands that can be executed in the console.
More information about the project can be found in the file ```SO TP2.pdf```, beware that this file is in spanish.