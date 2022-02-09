# Data structure description

The LCRQ is the foundation of the fastest modern concurrent fifo queues. It is a MS queue of queues segment arrays, where items are inserted into slots in the segments using fetch-and-add (to find the right index).

## Origin

It was introduced in the paper [Fast concurrent queues for X86 processors](https://doi.org/10.1145/2442516.2442527), and the implementation is from [https://github.com/chaoran/fast-wait-free-queue](https://github.com/chaoran/fast-wait-free-queue).

## Main author

Kåre von Geijer <karev@chalmers.se> (only integrated the queue with this testing framework)