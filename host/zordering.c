#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


typedef struct {
    int x, y;
} Point;

// Function to compute Z-value (Morton code) of a point
int Zval(Point P) {    
    int x = P.x;
    x = (x ^ (x << 16)) & 0x0000ffff0000ffff; 
    x = (x ^ (x << 8))  & 0x00ff00ff00ff00ff; 
    x = (x ^ (x << 4))  & 0x0f0f0f0f0f0f0f0f; 
    x = (x ^ (x << 2))  & 0x3333333333333333; 
    x = (x ^ (x << 1))  & 0x5555555555555555; 
    
    int y = P.y;
    y = (y ^ (y << 16)) & 0x0000ffff0000ffff; 
    y = (y ^ (y << 8))  & 0x00ff00ff00ff00ff; 
    y = (y ^ (y << 4))  & 0x0f0f0f0f0f0f0f0f; 
    y = (y ^ (y << 2))  & 0x3333333333333333; 
    y = (y ^ (y << 1))  & 0x5555555555555555; 
    
    return (y << 1) | x;
}

// Struct to store Z-value and index of each point for sorting
typedef struct {
    int z_value;
    int index;
} ZPoint;

// Comparison function for sorting ZPoints by Z-value
int compareZPoints(const void *a, const void *b) {
    return ((ZPoint*)a)->z_value - ((ZPoint*)b)->z_value;
}

// Function to sort points based on Z-values
void Zsorting(Point points[], int num_points) {
    ZPoint zpoints[num_points];
    
    // Compute Z-values and store them with indices
    for (int i = 0; i < num_points; i++) {
        zpoints[i].z_value = Zval(points[i]);
        zpoints[i].index = i;
    }
    
    // Sort ZPoints array based on Z-values
    qsort(zpoints, num_points, sizeof(ZPoint), compareZPoints);
    
    // Create a temporary array to store sorted points
    Point sorted_points[num_points];
    for (int i = 0; i < num_points; i++) {
        sorted_points[i] = points[zpoints[i].index];
    }
    
    // Copy sorted points back to the original points array
    for (int i = 0; i < num_points; i++) {
        points[i] = sorted_points[i];
    }
}

