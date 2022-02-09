# Data structure description

The k-segment queue is a relaxed queue implemented as a Michael-Scott queue of array segments of size k. Theses segments are filled up in fifo order, but without internal ordering within the segments (leading to k out-of-order relaxation).

## Origin

The design is the k-queue from the paper [Performance, Scalability, and Semantics of Concurrent FIFO Queues](https://doi.org/10.1007/978-3-642-33078-0_20), and the implementation is from the evaluation of the [first 2D paper](https://doi.org/10.4230/LIPIcs.DISC.2019.31).

## Main Author

Adones Rukundo
