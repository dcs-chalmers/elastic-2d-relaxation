# Data structure description

This is a benchmark of the compare-and-swap (CAS) instruction. All threads repeatedly try to update a counter using CAS, and the test measures the number of successful updates. This can be seen as an upper bound on performance for designs with a CAS bottleneck.

## Main Author

Kåre von Geijer <karev@chalmers.se>