This contains the development version of a C++-based code that will parse the MOM6 configuration files. This repo contains source, a Makefile, several test MOM6 input files, and a submodule for PEGTL[...]  

To clone the repository (including submodules), run:

```bash
git clone --recurse-submodules https://github.com/johnmauff/TIM_parser
```

To build the code, type:

```bash
make
```

The code can be run against several MOM6 configuration files found in the directory `tests`. For example, type:

```bash
mom6_parser tests/MOM_inputA
```