# Grayscale Bitmap Histogram Thresholding

Project of the course "Distributed Systems: Paradigm and Models " at the Master Course in Computer Science, at the University of Pisa, Academic year: 2017 / 2018.

### Text

This module performs histogram thresholding on an image.  Given an integer image I and a target percentage p, it constructs a binary image B such that B(i,j) is set if no more than p percent of the  pixels in I are brighter than I(i,j). The input data are therefore the matrix I and the percentage p. The thresholding must be applied to a stream of input images.

## Overview

(TODO)

## Prerequisites

C++11
 
### Third party tools

* [CImg](http://cimg.eu/) - *C++ Library for image processing* 
* [FastFlow](http://calvados.di.unipi.it/) - *C++ Framework for parallel programming* 

## Getting Started

(TODO)

```
[] = runInstance(gen_name, edge_name, idExp, i, toplot)
```

- gen_name : string is the name of the generator, in our case 'netgen' or 'complete', since we use these two;
- edge_name : string is the cardinality of the edges of the graphs, in our case the possibilities are '1000', '2000', '3000', '8000', '16000', '128000', '512000' and '1000000';
- idExp : int identifies the matrix D to use: 1 to use D = I; 2 to use "uniform" D; 3 to use "bad scaled" D;
- i : int the instance of the graph to use (ex: the 1st, the 6th etc..);
- toplot : string "yep" to plot how the residual decreases both for CG and preconditioned CG; plot anything if any other string is inserted.


## Authors

* **Andrea Lisi** - *Member* - [0Alic](https://github.com/0Alic)
