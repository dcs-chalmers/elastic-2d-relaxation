# Data structure description

The coupled 2D deque, which has a single window bounding all sub-deques in all direction, in some way.

However, (I, Kåre, think that) this approach is inherently flawed, and does not work. Instead, a 2Dc deque should have one window at each end of the deque, bounding the rows (upward and downward) each end can be within.

## Origin

Experiment for the [first 2D paper](https://doi.org/10.4230/LIPIcs.DISC.2019.31), but ultimately not used as it does not work.

## Main Author

Adones Rukundo