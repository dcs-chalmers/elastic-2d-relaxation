# Data structure description

The Treiber stack is the most foundational lock-free stack, designed as a linked list of queue nodes, operated on in lifo order. It only requires an atomic pointer to the top item in the stack, while all other pointers in the stack will be immutable after initialization.
## Origin

It was introduced in the report [Systems programming: Coping with parallelism](https://dominoweb.draco.res.ibm.com/reports/rj5118.pdf), and the implementation is from [https://github.com/LPD-EPFL/ASCYLIB](https://github.com/LPD-EPFL/ASCYLIB).

