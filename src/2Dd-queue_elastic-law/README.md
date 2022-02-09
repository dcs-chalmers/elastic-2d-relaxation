# Data structure description

The elastic Lateral-as-Window (LaW) 2D queue, which has two windows which bounds the number of enqueues (dequeues) at the tail (head) of each sub-queue. It encompasses elastic relaxation, and is able to change the window dimensions during run-time. By merging the Lateral and the Window (the Lateral becomes a queue of windows), it becomes simple to change window dimensions when enqueuing a new window. The drawback is that it can only change dimensions when enqueuing a new window, which is at the tail, and the head only has to adapt to the already enqueued windows.

## Origin

The [elastic 2D paper](https://arxiv.org/abs/2403.13644).

## Main Author

Kåre von Geijer <karev@chalmers.se>