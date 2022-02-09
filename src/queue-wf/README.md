# Data structure description

This fast wait-free queue is one of the fastest concurrent fifo queues, often performing as well as the LCRQ. It is a MS queue of queues segment arrays, where items are inserted into slots in the segments using fetch-and-add (to find the right index). It uses helping and a fast-path slow-path approach to guarantee the wait-freedom.

## Origin

It was introduced in the paper [A wait-free queue as fast as fetch-and-add](https://doi.org/10.1145/2851141.2851168), and the implementation is from [https://github.com/chaoran/fast-wait-free-queue](https://github.com/chaoran/fast-wait-free-queue).

## Main author

Kåre von Geijer <karev@chalmers.se> (only integrated the queue with this testing framework)