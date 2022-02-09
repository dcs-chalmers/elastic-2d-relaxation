# Data structure description

The elimination stack builds on the basic lock-free Treiber stack. But in front of the lock-free stack, there is an elimination array, where enqueues and dequeues try to collide (before going to the Treiber stack), which alleviates the bottle-neck of the top of the Treiber stack.

## Origin

The design is the k-queue from the paper [A scalable lock-free stack algorithm](https://doi.org/10.1016/j.jpdc.2009.08.011), and the implementation is from the evaluation of the [first 2D paper](https://doi.org/10.4230/LIPIcs.DISC.2019.31).

## Main Author

Adones Rukundo
