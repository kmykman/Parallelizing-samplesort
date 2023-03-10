# Parallelizing-samplesort
Perform parallelizing samplesort with multithreading and `qsort()` together to generate result faster when a large number of numbers are needed to be sorted

## Author
Man Yuet Ki Kimmy

## Description
There are 5 phases in total.
1. Each thread gets a subsequence with approximately π/π numbers and uses the quicksort algorithm to sort its subsequence.  After the sorting, each thread chooses π samples from its sorted subsequence at local indices 0, π/π^2 , 2π/π^2, ..., (π-1)π/π^2.
2. One thread gathers all samples and sorts them. Then, it selects p-1 pivot values from this sorted list of samples. The pivot values are selected at indices π + βπ/2β β 1, 2π + βπ/2β β 1, β― , (π β 1)π + βπ/2β β 1.
3. All threads get the π-1 pivot values, then each thread partitions its sorted subsequence into π disjoint pieces, using the pivot values as separators between pieces.
4. Thread π keeps its πth partition and collects from other threads their πth partitions and merges them into a single sequence
5. All threads sort their sequences and concatenate them to form the final sorted sequence.

## Features
- the numbers to be sorted is randomly generated
- if no <number of working thread> is inputed, the number of working thread is default to be 4
- if inputed `number of number needed to be sorted` < `number of working thread`, the number of working thread will be `number of number needed to be sorted`
- maximum number of `number of number needed to be sorted` and `number of working thread` depends on your executing computer or server

## Usage
```
gcc psort.c -o psort
./psort <number of number needed to be sorted> <number of working thread (default: 4)>
```

## Output
The program outputs the first, at 25%, at 75% and last number in the sorted number sequence and also the total time takes to sort the number list.
```
First : 2
At 25%: 536690108
At 50%: 1073542222
At 75%: 1610448550
Last : 2147483639
Total elapsed time: 19.6378 s
```

## Result
<img width="334" alt="θ’εΉζ·εη«ι’_20230220_205553" src="https://user-images.githubusercontent.com/78306170/220115374-75387693-0066-439b-b1ac-1560ffe41568.png">

Copyright 2022 Man Yuet Ki Kimmy
