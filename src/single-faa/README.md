# Data structure description

This is a benchmark of the fetch-and-add (FAA) instruction. All threads repeatedly try to update a counter using FAA, and the test measures the number of successful updates. This can be seen as an upper bound on performance for designs with a FAA bottleneck.

## Main Author

Kåre von Geijer <karev@chalmers.se>