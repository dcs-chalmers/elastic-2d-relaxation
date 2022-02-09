# Data structure description

The optimized decoupled 2D queue, which has two windows which bounds the number of enqueues (dequeues) at the tail (head) of each sub-queue. It works exactly like the normal 2Dd queue, but is optimized to keep up with the scalability of the elastic 2D queue implementations (no algorithmic changes).
## Origin

Design is from the [first 2D paper](https://doi.org/10.4230/LIPIcs.DISC.2019.31), and the implementation is from the [elastic 2D paper](https://arxiv.org/abs/2403.13644).

## Main Author

Kåre von Geijer <karev@chalmers.se>