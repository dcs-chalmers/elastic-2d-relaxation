# Data structure description

The random choice-of-d stack randomly selects d sub-stacks for every operations, proceeding to operate on the most suitable one, depending on the number of operations at the different sides of the stacks.

## Origin

The design is an adapted stack version of the d-RA queue from the paper [Distributed Queues in Shared Memory](https://doi.org/10.1145/2482767.2482789), and the implementation is derived from the random choice-of-d stack in the evaluation of the [first 2D paper](https://doi.org/10.4230/LIPIcs.DISC.2019.31).

## Main Author

Adones Rukundo
Kåre von Geijer