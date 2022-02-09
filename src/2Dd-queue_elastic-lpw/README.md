# Data structure description

The elastic Lateral-plus-Window (LpW) 2D queue, which has two windows which bounds the number of enqueues (dequeues) at the tail (head) of each sub-queue. It encompasses elastic relaxation, and is able to change the window dimensions during run-time. By keeping a Lateral queue to the side, it is able to track elastic changes in width. Both the head and tail can elastically change the depth, but only the tail is allowed to change width and has to adapt to the width information in the Lateral.

## Origin

The [elastic 2D paper](https://arxiv.org/abs/2403.13644).

## Main Author

Kåre von Geijer <karev@chalmers.se>